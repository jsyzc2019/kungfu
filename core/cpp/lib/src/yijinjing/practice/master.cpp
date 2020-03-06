//
// Created by Keren Dong on 2019-06-15.
//

#include <typeinfo>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <kungfu/longfist/longfist.h>
#include <kungfu/longfist/serialize/sql.h>
#include <kungfu/yijinjing/time.h>
#include <kungfu/yijinjing/practice/config_store.h>
#include <kungfu/yijinjing/practice/master.h>

using namespace kungfu::rx;
using namespace kungfu::longfist;
using namespace kungfu::longfist::types;
using namespace kungfu::longfist::sqlite;
using namespace kungfu::yijinjing;
using namespace kungfu::yijinjing::data;

namespace kungfu::yijinjing::practice
{

    master::master(location_ptr home, bool low_latency) :
            hero(std::make_shared<io_device_master>(home, low_latency)),
            start_time_(time::now_in_nano()), last_check_(0)
    {
        config_store cs(get_locator());
        for (auto item : cs.get_all(Config{}))
        {
            add_location(start_time_, location::make_shared(item.second, get_locator()));
        }
        auto io_device = std::dynamic_pointer_cast<io_device_master>(get_io_device());
        io_device->open_session(io_device->get_home(), start_time_);
        writers_.emplace(location::PUBLIC, io_device->open_writer(0));
        writers_.at(location::PUBLIC)->mark(start_time_, SessionStart::tag);
    }

    void master::on_exit()
    {
        auto io_device = std::dynamic_pointer_cast<io_device_master>(get_io_device());
        auto now = time::now_in_nano();
        io_device->close_session(io_device->get_home(), now);
        writers_.at(location::PUBLIC)->mark(now, SessionEnd::tag);
    }

    void master::on_notify()
    {
        get_io_device()->get_publisher()->notify();
    }

    void master::register_app(const event_ptr &e)
    {
        auto io_device = std::dynamic_pointer_cast<io_device_master>(get_io_device());
        auto home = io_device->get_home();

        auto request_json = e->data<nlohmann::json>();
        auto request_data = e->data_as_string();

        Register register_data(request_data.c_str(), request_data.length());

        auto app_location = location::make_shared(register_data, home->locator);

        if (is_location_live(app_location->uid))
        {
            SPDLOG_ERROR("location {} has already been registered live", app_location->uname);
            return;
        }

        auto now = time::now_in_nano();
        auto uid_str = fmt::format("{:08x}", app_location->uid);
        auto master_cmd_location = std::make_shared<location>(mode::LIVE, category::SYSTEM, "master", uid_str, home->locator);

        add_location(e->gen_time(), app_location);
        add_location(e->gen_time(), master_cmd_location);
        register_location(e->gen_time(), register_data);

        app_locations_.emplace(app_location->uid, master_cmd_location->uid);
        writers_.emplace(app_location->uid, get_io_device()->open_writer_at(master_cmd_location, app_location->uid));
        reader_->join(app_location, location::PUBLIC, now);
        reader_->join(app_location, master_cmd_location->uid, now);

        writers_.at(location::PUBLIC)->write(e->gen_time(), register_data);

        auto writer = writers_.at(app_location->uid);

        io_device->open_session(app_location, e->gen_time());
        writer->mark(e->gen_time(), SessionStart::tag);

        require_write_to(e->gen_time(), app_location->uid, location::PUBLIC);
        require_write_to(e->gen_time(), app_location->uid, master_cmd_location->uid);

        write_trading_day(e->gen_time(), writer);
        write_locations(e->gen_time(), writer);

        app_sqlizers_.emplace(app_location->uid, std::make_shared<sqlizer>(app_location));
        app_sqlizers_.at(app_location->uid)->restore(writer);

        writer->mark(start_time_, RequestStart::tag);

        write_registers(e->gen_time(), writer);
        write_channels(e->gen_time(), writer);

        on_register(e, app_location);
    }

    void master::deregister_app(int64_t trigger_time, uint32_t app_location_uid)
    {
        auto location = get_location(app_location_uid);
        auto io_device = std::dynamic_pointer_cast<io_device_master>(get_io_device());
        writers_[app_location_uid]->mark(trigger_time, SessionEnd::tag);
        io_device->close_session(location, trigger_time);
        deregister_channel_by_source(app_location_uid);
        deregister_location(trigger_time, app_location_uid);
        reader_->disjoin(app_location_uid);
        registry_.erase(app_location_uid);
        writers_.erase(app_location_uid);
        timer_tasks_.erase(app_location_uid);
        app_sqlizers_.erase(app_location_uid);
        writers_.at(location::PUBLIC)->write(trigger_time, location->to<Deregister>());
    }

    void master::publish_trading_day()
    {
        write_trading_day(0, writers_.at(location::PUBLIC));
    }

    bool master::produce_one(const rx::subscriber<event_ptr> &sb)
    {
        auto now = time::now_in_nano();

        for (auto &app : timer_tasks_)
        {
            uint32_t app_id = app.first;
            auto &app_tasks = app.second;
            for (auto it = app_tasks.begin(); it != app_tasks.end();)
            {
                auto &task = it->second;
                if (task.checkpoint <= now)
                {
                    writers_[app_id]->mark(0, Time::tag);
                    SPDLOG_DEBUG("sent time event to {}", get_location(app_id)->uname);
                    task.checkpoint += task.duration;
                    task.repeat_count++;
                    if (task.repeat_count >= task.repeat_limit)
                    {
                        it = app_tasks.erase(it);
                        continue;
                    }
                }
                it++;
            }
        }

        if (last_check_ + time_unit::NANOSECONDS_PER_SECOND < now)
        {
            on_interval_check(now);
            last_check_ = now;
        }

        return hero::produce_one(sb);
    }

    void master::react()
    {
        events_ | instanceof<journal::frame>() |
        $([&](const event_ptr &e)
          {
              auto io_device = std::dynamic_pointer_cast<io_device_master>(get_io_device());
              io_device->update_session(std::dynamic_pointer_cast<journal::frame>(e));
              cast_event_invoke(e, *app_sqlizers_.at(e->source()));
          });

        events_ | is(Ping::tag) |
        $([&](const event_ptr &e)
          {
              get_io_device()->get_publisher()->publish("{}");
          });

        events_ | is(Location::tag) |
        $([&](const event_ptr &e)
          {
              Location data = e->data<Location>();
              add_location(e->gen_time(), location::make_shared(data, get_locator()));
              writers_.at(location::PUBLIC)->write(e->gen_time(), data);
          });

        events_ | is(Register::tag) | $$(register_app);

        events_ | is(RequestWriteTo::tag) |
        $([&](const event_ptr &e)
          {
              const RequestWriteTo &request = e->data<RequestWriteTo>();
              if (check_location_live(e->source(), request.dest_id))
              {
                  reader_->join(get_location(e->source()), request.dest_id, e->gen_time());
                  require_write_to(e->gen_time(), e->source(), request.dest_id);
                  require_read_from(0, request.dest_id, e->source(), e->gen_time());
                  Channel channel = {};
                  channel.source_id = e->source();
                  channel.dest_id = request.dest_id;
                  register_channel(e->gen_time(), channel);
                  writers_.at(location::PUBLIC)->write(e->gen_time(), channel);
              }
          });

        events_ | is(RequestReadFrom::tag) |
        $([&](const event_ptr &e)
          {
              const RequestReadFrom &request = e->data<RequestReadFrom>();
              if (check_location_live(request.source_id, e->source()))
              {
                  reader_->join(get_location(request.source_id), e->source(), e->gen_time());
                  require_write_to(e->gen_time(), request.source_id, e->source());
                  require_read_from(e->gen_time(), e->source(), request.source_id, request.from_time);
                  Channel channel = {};
                  channel.source_id = request.source_id;
                  channel.dest_id = e->source();
                  register_channel(e->gen_time(), channel);
                  writers_.at(location::PUBLIC)->write(e->gen_time(), channel);
              }
          });

        events_ | is(RequestReadFromPublic::tag) |
        $([&](const event_ptr &e)
          {
              const RequestReadFromPublic &request = e->data<RequestReadFromPublic>();
              require_read_from_public(e->gen_time(), e->source(), request.source_id, request.from_time);
          });

        events_ | is(TimeRequest::tag) |
        $([&](const event_ptr &e)
          {
              const TimeRequest &request = e->data<TimeRequest>();
              if (timer_tasks_.find(e->source()) == timer_tasks_.end())
              {
                  timer_tasks_[e->source()] = std::unordered_map<int32_t, timer_task>();
              }
              std::unordered_map<int32_t, timer_task> &app_tasks = timer_tasks_[e->source()];
              if (app_tasks.find(request.id) == app_tasks.end())
              {
                  app_tasks[request.id] = timer_task();
              }
              timer_task &task = app_tasks[request.id];
              task.checkpoint = time::now_in_nano() + request.duration;
              task.duration = request.duration;
              task.repeat_count = 0;
              task.repeat_limit = request.repeat;
              SPDLOG_TRACE("time request from {} duration {} repeat {} at {}",
                           get_location(e->source())->uname, request.duration, request.repeat, time::strftime(e->gen_time()));
          });
    }

    void master::write_trading_day(int64_t trigger_time, const yijinjing::journal::writer_ptr &writer)
    {
        TradingDay &trading_day = writer->open_data<TradingDay>();
        trading_day.timestamp = acquire_trading_day();
        writer->close_data();
    }

    void master::write_locations(int64_t trigger_time, const yijinjing::journal::writer_ptr &writer)
    {
        for (const auto &item : locations_)
        {
            writer->write(trigger_time, dynamic_cast<Location &>(*item.second));
        }
    }

    void master::write_registers(int64_t trigger_time, const yijinjing::journal::writer_ptr &writer)
    {
        for (const auto &item : registry_)
        {
            writer->write(trigger_time, item.second);
        }
    }

    void master::write_channels(int64_t trigger_time, const yijinjing::journal::writer_ptr &writer)
    {
        for (const auto &item: channels_)
        {
            writer->write(trigger_time, item.second);
        }
    }
}

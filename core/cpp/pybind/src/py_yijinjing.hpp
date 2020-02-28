//
// Created by Keren Dong on 2020/1/30.
//

#ifndef KUNGFU_PY_YIJINJING_HPP
#define KUNGFU_PY_YIJINJING_HPP

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <kungfu/longfist/longfist.h>
#include <kungfu/yijinjing/io.h>
#include <kungfu/yijinjing/time.h>
#include <kungfu/yijinjing/log/setup.h>
#include <kungfu/yijinjing/journal/journal.h>
#include <kungfu/yijinjing/journal/frame.h>
#include <kungfu/yijinjing/nanomsg/socket.h>
#include <kungfu/yijinjing/util/util.h>
#include <kungfu/yijinjing/practice/master.h>
#include <kungfu/yijinjing/practice/apprentice.h>
#include <kungfu/yijinjing/practice/config_store.h>

namespace kungfu::yijinjing
{
    namespace py = pybind11;

    using namespace kungfu;
    using namespace kungfu::longfist::types;
    using namespace kungfu::yijinjing;
    using namespace kungfu::yijinjing::journal;
    using namespace kungfu::yijinjing::nanomsg;
    using namespace kungfu::yijinjing::util;
    using namespace kungfu::yijinjing::practice;

    class PyLocator : public data::locator
    {
        using data::locator::locator;

        [[nodiscard]] bool has_env(const std::string &name) const override
        {
            PYBIND11_OVERLOAD_PURE(bool, data::locator, has_env, name)
        }

        [[nodiscard]] std::string get_env(const std::string &name) const override
        {
            PYBIND11_OVERLOAD_PURE(std::string, data::locator, get_env, name)
        }

        [[nodiscard]] std::string layout_dir(data::location_ptr location, longfist::enums::layout l) const override
        {
            PYBIND11_OVERLOAD_PURE(std::string, data::locator, layout_dir, location, l)
        }

        [[nodiscard]] std::string layout_file(data::location_ptr location, longfist::enums::layout l, const std::string &name) const override
        {
            PYBIND11_OVERLOAD_PURE(std::string, data::locator, layout_file, location, l, name)
        }

        [[nodiscard]] std::string default_to_system_db(data::location_ptr location, const std::string &name) const override
        {
            PYBIND11_OVERLOAD_PURE(std::string, data::locator, default_to_system_db, location, name)
        }

        [[nodiscard]] std::vector<int> list_page_id(data::location_ptr location, uint32_t dest_id) const override
        {
            PYBIND11_OVERLOAD_PURE(std::vector<int>, data::locator, list_page_id, location, dest_id)
        }


        [[nodiscard]] std::vector<data::location_ptr>
        list_locations(const std::string &category, const std::string &group, const std::string &name, const std::string &mode) const override
        {
            PYBIND11_OVERLOAD_PURE(std::vector<data::location_ptr>, data::locator, list_locations, category, group, name, mode)
        }

        [[nodiscard]] std::vector<uint32_t> list_location_dest(data::location_ptr location) const override
        {
            PYBIND11_OVERLOAD_PURE(std::vector<uint32_t>, data::locator, list_location_dest, location)
        }
    };

    class PyEvent : public event
    {
    public:
        [[nodiscard]] int64_t gen_time() const override
        {
            PYBIND11_OVERLOAD_PURE(int64_t, event, gen_time,)
        }

        [[nodiscard]] int64_t trigger_time() const override
        {
            PYBIND11_OVERLOAD_PURE(int64_t, event, trigger_time,)
        }

        [[nodiscard]] int32_t msg_type() const override
        {
            PYBIND11_OVERLOAD_PURE(int64_t, event, msg_type,)
        }

        [[nodiscard]] uint32_t source() const override
        {
            PYBIND11_OVERLOAD_PURE(int64_t, event, source,)
        }

        [[nodiscard]] uint32_t dest() const override
        {
            PYBIND11_OVERLOAD_PURE(int64_t, event, dest,)
        }
    };

    class PyPublisher : public publisher
    {
    public:
        int notify() override
        {
            PYBIND11_OVERLOAD_PURE(int, publisher, notify,)
        }

        int publish(const std::string &json_message) override
        {
            PYBIND11_OVERLOAD_PURE(int, publisher, publish, json_message)
        }
    };

    class PyObserver : public observer
    {
    public:
        bool wait() override
        {
            PYBIND11_OVERLOAD_PURE(bool, observer, wait,)
        }

        const std::string &get_notice() override
        {
            PYBIND11_OVERLOAD_PURE(const std::string &, observer, get_notice,)
        }
    };

    class PyMaster : public master
    {
    public:
        using master::master;

        void on_register(const event_ptr &event, const data::location_ptr &app_location) override
        {
            PYBIND11_OVERLOAD(void, master, on_register, event, app_location);
        }

        void on_interval_check(int64_t nanotime) override
        {
            PYBIND11_OVERLOAD(void, master, on_interval_check, nanotime);
        }

        void on_exit() override
        {
            PYBIND11_OVERLOAD(void, master, on_exit);
        }
    };

    class PyApprentice : public apprentice
    {
    public:
        using apprentice::apprentice;

        void on_trading_day(const event_ptr &event, int64_t daytime) override
        {
            PYBIND11_OVERLOAD(void, apprentice, on_trading_day, event, daytime);
        }
    };

    RequestReadFrom get_RequestReadFrom(const frame_ptr &f)
    {
        return f->data<RequestReadFrom>();
    }

    void bind(pybind11::module &&m)
    {
        yijinjing::ensure_sqlite_initilize();

        m.def("thread_id", &get_thread_id);
        m.def("in_color_terminal", &in_color_terminal);
        m.def("color_print", &color_print);

        // nanosecond-time related
        m.def("now_in_nano", &time::now_in_nano);
        m.def("strftime", &time::strftime, py::arg("nanotime"), py::arg("format") = KUNGFU_DATETIME_FORMAT_DEFAULT);
        m.def("strptime", &time::strptime, py::arg("timestr"), py::arg("format") = KUNGFU_DATETIME_FORMAT_DEFAULT);
        m.def("strfnow", &time::strfnow, py::arg("format") = KUNGFU_DATETIME_FORMAT_DEFAULT);

        m.def("setup_log", &kungfu::yijinjing::log::setup_log);

        m.def("hash_32", &hash_32, py::arg("key"), py::arg("length"), py::arg("seed") = KUNGFU_HASH_SEED);
        m.def("hash_str_32", &hash_str_32, py::arg("key"), py::arg("seed") = KUNGFU_HASH_SEED);
        m.def("get_page_path", &page::get_page_path);

        py::enum_<longfist::enums::mode>(m, "mode", py::arithmetic(), "Kungfu Run Mode")
                .value("LIVE", longfist::enums::mode::LIVE)
                .value("DATA", longfist::enums::mode::DATA)
                .value("REPLAY", longfist::enums::mode::REPLAY)
                .value("BACKTEST", longfist::enums::mode::BACKTEST)
                .export_values();
        m.def("get_mode_name", &longfist::enums::get_mode_name);
        m.def("get_mode_by_name", &longfist::enums::get_mode_by_name);

        py::enum_<longfist::enums::category>(m, "category", py::arithmetic(), "Kungfu Data Category")
                .value("MD", longfist::enums::category::MD)
                .value("TD", longfist::enums::category::TD)
                .value("STRATEGY", longfist::enums::category::STRATEGY)
                .value("SYSTEM", longfist::enums::category::SYSTEM)
                .export_values();
        m.def("get_category_name", &longfist::enums::get_category_name);

        py::enum_<longfist::enums::layout>(m, "layout", py::arithmetic(), "Kungfu Data Layout")
                .value("JOURNAL", longfist::enums::layout::JOURNAL)
                .value("SQLITE", longfist::enums::layout::SQLITE)
                .value("NANOMSG", longfist::enums::layout::NANOMSG)
                .value("LOG", longfist::enums::layout::LOG)
                .export_values();
        m.def("get_layout_name", &longfist::enums::get_layout_name);

        py::class_<event, PyEvent, std::shared_ptr<event>>(m, "event")
                .def_property_readonly("gen_time", &event::gen_time)
                .def_property_readonly("trigger_time", &event::trigger_time)
                .def_property_readonly("source", &event::source)
                .def_property_readonly("dest", &event::dest)
                .def_property_readonly("msg_type", &event::msg_type)
                .def_property_readonly("data_length", &event::data_length)
                .def_property_readonly("data_as_bytes", &event::data_as_bytes)
                .def_property_readonly("data_as_string", &event::data_as_string)
                .def("to_string", &event::to_string);

        py::class_<frame, event, frame_ptr>(m, "frame")
                .def_property_readonly("gen_time", &frame::gen_time)
                .def_property_readonly("trigger_time", &frame::trigger_time)
                .def_property_readonly("source", &frame::source)
                .def_property_readonly("dest", &frame::dest)
                .def_property_readonly("msg_type", &frame::msg_type)
                .def_property_readonly("frame_length", &frame::frame_length)
                .def_property_readonly("header_length", &frame::header_length)
                .def_property_readonly("data_length", &frame::data_length)
                .def_property_readonly("address", &frame::address)
                .def_property_readonly("data_as_bytes", &frame::data_as_bytes)
                .def_property_readonly("data_as_string", &frame::data_as_string)
                .def("has_data", &frame::has_data)
                .def_property_readonly("data_address", [](const frame &f)
                { return f.address() + f.header_length(); });

        auto location_class = py::class_<data::location, std::shared_ptr<data::location>>(m, "location");
        location_class.def(py::init<longfist::enums::mode, longfist::enums::category, const std::string &, const std::string &, data::locator_ptr>())
                .def_readonly("mode", &data::location::mode)
                .def_readonly("category", &data::location::category)
                .def_readonly("group", &data::location::group)
                .def_readonly("name", &data::location::name)
                .def_readonly("uname", &data::location::uname)
                .def_readonly("uid", &data::location::uid)
                .def_readonly("locator", &data::location::locator)
                .def("__repr__", [&](data::location &target)
                {
                    return target.uname;
                });
        location_class.def("to", py::overload_cast<longfist::types::Config &>(&data::location::to<longfist::types::Config>, py::const_));
        location_class.def("to", py::overload_cast<longfist::types::Register &>(&data::location::to<longfist::types::Register>, py::const_));
        location_class.def("to", py::overload_cast<longfist::types::Deregister &>(&data::location::to<longfist::types::Deregister>, py::const_));
        location_class.def("to", py::overload_cast<longfist::types::Location &>(&data::location::to<longfist::types::Location>, py::const_));

        py::class_<data::locator, PyLocator, std::shared_ptr<data::locator>>(m, "locator")
                .def(py::init())
                .def("has_env", &data::locator::has_env)
                .def("get_env", &data::locator::get_env)
                .def("layout_dir", &data::locator::layout_dir)
                .def("layout_file", &data::locator::layout_file)
                .def("list_page_id", &data::locator::list_page_id)
                .def("list_locations", &data::locator::list_locations, py::arg("category") = "*", py::arg("group") = "*", py::arg("name") = "*",
                     py::arg("mode") = "*")
                .def("list_location_dest", &data::locator::list_location_dest);

        py::enum_<nanomsg::protocol>(m, "protocol", py::arithmetic(), "Nanomsg Protocol")
                .value("REPLY", nanomsg::protocol::REPLY)
                .value("REQUEST", nanomsg::protocol::REQUEST)
                .value("PUSH", nanomsg::protocol::PUSH)
                .value("PULL", nanomsg::protocol::PULL)
                .value("PUBLISH", nanomsg::protocol::PUBLISH)
                .value("SUBSCRIBE", nanomsg::protocol::SUBSCRIBE)
                .export_values();

        py::class_<socket, socket_ptr>(m, "socket")
                .def(py::init<protocol>(), py::arg("protocol"))
                .def("setsockopt", &socket::setsockopt_str, py::arg("level"), py::arg("option"), py::arg("value"))
                .def("setsockopt", &socket::setsockopt_int, py::arg("level"), py::arg("option"), py::arg("value"))
                .def("getsockopt", &socket::getsockopt_int, py::arg("level"), py::arg("option"))
                .def("bind", &socket::bind, py::arg("url"))
                .def("connect", &socket::connect, py::arg("url"))
                .def("shutdown", &socket::shutdown, py::arg("how") = 0)
                .def("close", &socket::close)
                .def("send", &socket::send, py::arg("msg"), py::arg("flags") = 0)
                .def("recv", &socket::recv_msg, py::arg("flags") = 0)
                .def("last_message", &socket::last_message);

        py::class_<publisher, PyPublisher, publisher_ptr>(m, "publisher")
                .def("publish", &publisher::publish)
                .def("notify", &publisher::notify);

        py::class_<observer, PyObserver, observer_ptr>(m, "observer")
                .def("wait", &observer::wait)
                .def("get_notice", &observer::get_notice);

        py::class_<reader, reader_ptr>(m, "reader")
                .def("subscribe", &reader::join)
                .def("current_frame", &reader::current_frame)
                .def("seek_to_time", &reader::seek_to_time)
                .def("data_available", &reader::data_available)
                .def("next", &reader::next)
                .def("join", &reader::join)
                .def("disjoin", &reader::disjoin);

        auto py_writer = py::class_<writer, writer_ptr>(m, "writer");
        py_writer.def("write_raw", &writer::write_raw)
                .def("write_str",
                     [](const writer_ptr &w, int64_t trigger_time, int32_t msg_type, const std::string &data)
                     {
                         w->write_raw(trigger_time, msg_type, reinterpret_cast<uintptr_t>(data.c_str()), data.length());
                     })
                .def("current_frame_uid", &writer::current_frame_uid)
                .def("mark", &writer::mark)
                .def("mark_with_time", &writer::mark_with_time);

        hana::for_each(longfist::StateDataTypes, [&](auto type)
        {
            using DataType = typename decltype(+hana::second(type))::type;
            py_writer.def("write", py::overload_cast<int64_t, const DataType &>(&writer::write<DataType>));
        });

        py::class_<io_device, io_device_ptr> io_device(m, "io_device");
        io_device.def(py::init<data::location_ptr, bool, bool>(), py::arg("location"), py::arg("low_latency") = false, py::arg("lazy") = true)
                .def_property_readonly("publisher", &io_device::get_publisher)
                .def_property_readonly("observer", &io_device::get_observer)
                .def_property_readonly("home", &io_device::get_home)
                .def_property_readonly("live_home", &io_device::get_live_home)
                .def("open_reader", &io_device::open_reader)
                .def("open_reader_to_subscribe", &io_device::open_reader_to_subscribe)
                .def("open_writer", &io_device::open_writer)
                .def("connect_socket", &io_device::connect_socket, py::arg("location"), py::arg("protocol"), py::arg("timeout") = 0)
                .def("find_sessions", &io_device::find_sessions, py::arg("source") = 0, py::arg("from") = 0, py::arg("to") = INT64_MAX);

        py::class_<io_device_with_reply, io_device_with_reply_ptr> io_device_with_reply(m, "io_device_with_reply", io_device);
        io_device_with_reply.def(py::init<data::location_ptr, bool, bool>());

        py::class_<io_device_master, io_device_master_ptr>(m, "io_device_master", io_device_with_reply)
                .def(py::init<data::location_ptr, bool>())
                .def("rebuild_index_db", &io_device_master::rebuild_index_db);

        py::class_<io_device_client, io_device_client_ptr>(m, "io_device_client", io_device_with_reply)
                .def(py::init<data::location_ptr, bool>());

        py::class_<master, PyMaster>(m, "master")
                .def(py::init<data::location_ptr, bool>(), py::arg("home"), py::arg("low_latency") = false)
                .def_property_readonly("io_device", &master::get_io_device)
                .def("now", &master::now)
                .def("get_home_uid", &master::get_home_uid)
                .def("get_live_home_uid", &master::get_live_home_uid)
                .def("get_home_uname", &master::get_home_uname)
                .def("has_location", &master::has_location)
                .def("get_location", &master::get_location)
                .def("has_writer", &master::has_writer)
                .def("get_writer", &master::get_writer)
                .def("run", &master::run)
                .def("publish_time", &master::publish_time)
                .def("send_time", &master::send_time)
                .def("on_register", &master::on_register)
                .def("on_interval_check", &master::on_interval_check)
                .def("on_exit", &master::on_exit)
                .def("deregister_app", &master::deregister_app);

        py::class_<apprentice, PyApprentice, apprentice_ptr>(m, "apprentice")
                .def(py::init<data::location_ptr, bool>(), py::arg("home"), py::arg("low_latency") = false)
                .def_property_readonly("io_device", &apprentice::get_io_device)
                .def("set_begin_time", &apprentice::set_begin_time)
                .def("set_end_time", &apprentice::set_end_time)
                .def("on_trading_day", &apprentice::on_trading_day)
                .def("run", &apprentice::run);

        auto cs_class = py::class_<config_store>(m, "config_store");
        cs_class.def(py::init<const yijinjing::data::locator_ptr&>());
        hana::for_each(longfist::ConfigDataTypes, [&](auto type)
        {
            using DataType = typename decltype(+hana::second(type))::type;
            cs_class.def("set", &config_store::set<DataType>);
            cs_class.def("get", &config_store::get<DataType>);
            cs_class.def("get_all", &config_store::get_all<DataType>);
            cs_class.def("remove", &config_store::remove<DataType>);
        });
    }
}

#endif //KUNGFU_PY_YIJINJING_HPP

#include "client/client.h"

void Client::close_connection() {
    mtx_sock.lock();
    if(sock.is_open()) {
        boost::system::error_code ec;
        sock.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
        if (ec) {
            std::cout << "Error when shutdown socket." << std::endl;
        }
        sock.close(ec);
        if (ec) {
            std::cout << "Error when close socket." << std::endl;
        }
        std::cout << "Close socket." << std::endl;
    }
    mtx_sock.unlock();
}

void Client::do_connect(std::vector<uint8_t>&&  __buffer) {
    std::cout << "start do_connect()" << std::endl;
    boost::asio::async_connect(sock, eps,
        [this, ptr_buffer = std::move(__buffer)](boost::system::error_code ec, boost::asio::ip::tcp::endpoint) mutable {
           if (!ec) {
               send_login_request(std::move(ptr_buffer));
               std::cout << "success finish do_connect()" << std::endl;
           }
           else {
               std::cout << "error do_connect()" << std::endl;
           }
    });
}

void Client::send_login_request(std::vector<uint8_t>&& __buffer) {
    std::cout << "send_login_request()" << std::endl;

    boost::asio::async_write(sock, boost::asio::buffer(__buffer.data(), __buffer.size()),
                                     [this](boost::system::error_code ec, std::size_t nbytes) {
            if (!ec) {
                std::cout << "send login_request=" << nbytes << " bytes" << std::endl;
                async_read_pb_header();

            } else {
                std::cout << "error send_login_request(header)" << std::endl;
                close_connection();
            }
    });
}

void Client::async_read_pb_header() {
    std::cout << "called async_read_pb_header()" << std::endl;
    boost::asio::async_read(sock, boost::asio::buffer(bin_buffer.data(), Protocol::SIZE_HEADER),
                            std::bind(&Client::do_read_pb_header,
                                      shared_from_this(),
                                      std::placeholders::_1,
                                      std::placeholders::_2));

}

void Client::do_read_pb_header(boost::system::error_code error, std::size_t nbytes) {
    if (!error) {
        std::cout << "new header read: " << nbytes << " bytes" << std::endl;

        Serialize::Header new_header;
        bool flag = new_header.ParseFromArray(bin_buffer.data(), Protocol::SIZE_HEADER);
        if (flag) {
            std::cout << "parse header: OK" << std::endl;
        } else {
            std::cout << "parse header: FAIL" << std::endl;
            std::cout << new_header << std::endl;
            return ;
        }
        async_read_pb_msg(new_header);

    } else {
        std::cout << "error read_pb_request_header()" << std::endl;
        close_connection();
    }
}

void Client::async_read_pb_msg(Serialize::Header new_header) {
    __read_buffer.resize(new_header.length());
    switch (static_cast<TypeCommand>(new_header.command())) {
        case TypeCommand::AutorisationResponse:
            boost::asio::async_read(sock, boost::asio::buffer(__read_buffer),
                                      std::bind(&Client::do_read_input_response,
                                                shared_from_this(),
                                                std::placeholders::_1,
                                                std::placeholders::_2));
        break;
        case TypeCommand::RegistrationResponse:
            boost::asio::async_read(sock, boost::asio::buffer(__read_buffer),
                                      std::bind(&Client::do_read_reg_response,
                                                shared_from_this(),
                                                std::placeholders::_1,
                                                std::placeholders::_2));
        break;
        case TypeCommand::EchoRequest:
            std::cout << "EchoRequest: " << std::endl;
        break;
        case TypeCommand::EchoResponse:
            boost::asio::async_read(sock, boost::asio::buffer(__read_buffer),
                                      std::bind(&Client::do_read_echo_response,
                                                shared_from_this(),
                                                std::placeholders::_1,
                                                std::placeholders::_2));
        break;
        case TypeCommand::JoinRoomResponse:
            boost::asio::async_read(sock, boost::asio::buffer(__read_buffer),
                                      std::bind(&Client::do_read_join_room_response,
                                                shared_from_this(),
                                                std::placeholders::_1,
                                                std::placeholders::_2));

        break;
        default:
            std::cout << "Unknown command " << new_header.command() << std::endl;
            break;
    }
}

void Client::do_read_input_response(boost::system::error_code error, std::size_t nbytes) {
    if (!error) {
        std::cout << "read responser=" << nbytes << " bytes" << std::endl;

        Serialize::Response response;
        bool flag_parse = response.ParseFromArray(__read_buffer.data(), static_cast<int>(__read_buffer.size()));
        if (flag_parse) {
            std::cout << "parse response: OK" << std::endl;
        } else {
            std::cout << "parse response: FAIL" << std::endl;
            close_connection();
            return ;
        }

        if (response.has_input_response()) {
            std::cout << "response include input_response" << std::endl;
            if (response.input_response().status() == Serialize::STATUS::OK) {
                std::cout << "Autorisation response: OK" << std::endl;
                emit send_input_code(InputCode::AutorOK);
            } else {
                emit send_input_code(InputCode::IncorrectAutor);
                std::cout << "Not found login/password" << std::endl;
                close_connection();
                return;
            }

            set_login_id(response.input_response().client_id());
            change_room(room_id);

            std::cout << "Success send_join_room_request()" << std::endl;
            async_read_pb_header();

        } else {
            std::cout << "response not include input_response" << std::endl;
        }
    }
    else {
        std::cout << "error when read response(data)" << std::endl;
        close_connection();
        return ;
    }
}

void Client::do_read_reg_response(boost::system::error_code error, std::size_t nbytes) {
    if (!error) {
        std::cout << "read responser=" << nbytes << " bytes" << std::endl;

        Serialize::Response response;
        bool flag_parse = response.ParseFromArray(__read_buffer.data(), static_cast<int>(__read_buffer.size()));
        if (flag_parse) {
            std::cout << "parse response: OK" << std::endl;
        } else {
            std::cout << "parse response: FAIL" << std::endl;
            close_connection();
            return ;
        }

        if (response.has_reg_response()) {
            std::cout << "response include reg_response" << std::endl;
            if (response.reg_response().status() == Serialize::STATUS::OK) {
                std::cout << "Registration response: OK" << std::endl;
                emit send_input_code(InputCode::RegistrOK);
            } else {
                emit send_input_code(InputCode::BusyRegistr);
                std::cout << "Registration response: FAIL, busy login" << std::endl;
                close_connection();
                return;
            }

            set_login_id(response.input_response().client_id());
            change_room(room_id);

            std::cout << "Success send_join_room_request()" << std::endl;
            async_read_pb_header();

        } else {
            std::cout << "response not include reg_response" << std::endl;
        }
    }
    else {
        std::cout << "error when read response(data)" << std::endl;
        close_connection();
        return ;
    }
}

void Client::do_read_join_room_response(boost::system::error_code error, std::size_t nbytes) {
    if (!error) {
        std::cout << "read responser=" << nbytes << " bytes" << std::endl;

        Serialize::Response response;
        bool flag_parse = response.ParseFromArray(__read_buffer.data(), static_cast<int>(__read_buffer.size()));
        if (flag_parse) {
            std::cout << "parse response: OK" << std::endl;
        } else {
            std::cout << "parse response: FAIL" << std::endl;
            close_connection();
            return ;
        }

        if (response.has_join_room_response()) {
            std::cout << "response include join_room" << std::endl;
            if (response.join_room_response().status() == Serialize::STATUS::OK) {
                std::cout << "Response join_room: OK" << std::endl;
                room_id = response.join_room_response().room_id();
            } else {
                std::cout << "Response join_room: FAIL" << std::endl;
                close_connection();
                return;
            }
            async_read_pb_header();
        } else {
            std::cout << "response not include join_room" << std::endl;
        }
    }
    else {
        std::cout << "error when read response(data)" << std::endl;
        close_connection();
        return ;
    }
}

void Client::do_read_echo_response(boost::system::error_code error, std::size_t) {
    if (!error) {
        Serialize::Response new_response;
        bool flag_parse = new_response.ParseFromArray(__read_buffer.data(), static_cast<int>(__read_buffer.size()));
        if (flag_parse) {
            std::cout << "parse echo_response: OK" << std::endl;
        } else {
            std::cout << "parse echo_response: FAIL" << std::endl;
        }
        std::cout << ">>>" << new_response.text_response().room_id()
                  << new_response.text_response().login()
                  << ": " << new_response.text_response().text() << std::endl;
        send_text(new_response.text_response().login(), new_response.text_response().text(), DateTime());
        async_read_pb_header();
    }
    else {
        std::cout << "Error read_pb_text_res()" << std::endl;
        close_connection();
    }
}


void Client::add_msg_to_send(std::vector<uint8_t>&& request_ptr) {
    bool process_write = !msg_to_server.empty();
    msg_to_server.push(std::move(request_ptr));

    if (!process_write) {
        start_send_msgs();
    }
}

void Client::start_send_msgs() {
    boost::asio::async_write(sock, boost::asio::buffer(msg_to_server.front()),
        [this](boost::system::error_code ec, std::size_t) {
        if (!ec) {
                msg_to_server.pop();
                if (!msg_to_server.empty()) {
                    start_send_msgs();
                }
        }
        else {
            std::cout << "Error send_request_header()" << std::endl;
            close_connection();
        }
    });
}

void Client::change_room(int new_room_id) {
    std::cout << "called change_room: " << new_room_id << std::endl;
    auto request_ptr = Protocol::MsgFactory::join_room_request(new_room_id);
    auto header_ptr = Protocol::MsgFactory::create_header(TypeCommand::JoinRoomRequest, request_ptr->ByteSizeLong());
    auto a_bin_buffer = Protocol::MsgFactory::serialize_request(std::move(header_ptr), std::move(request_ptr));

    add_msg_to_send(std::move(a_bin_buffer));
}

void Client::send_msg_to_server(const std::string& text, int _room_id) {
//    std::cout << "send_msg_to_server(): login=" << login << ", text=" << text << std::endl;
    auto request_ptr = Protocol::MsgFactory::create_text_request(login, _room_id, text);
    auto header_ptr = Protocol::MsgFactory::create_header(TypeCommand::EchoRequest, request_ptr->ByteSizeLong());
    auto a_bin_buffer = Protocol::MsgFactory::serialize_request(std::move(header_ptr), std::move(request_ptr));

    add_msg_to_send(std::move(a_bin_buffer));
}


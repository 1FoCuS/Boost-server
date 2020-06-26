#include <client/client.h>

void Client::write(Message mes) {
    mes.set_login_id(client_id);
    mes.set_login(login);
//    std::cout << "write: " << mes.get_login_id() << std::endl;
    mes.set_room_id(room_id);

    bool process_write = !sending_message.empty();
    sending_message.push_back(mes);

    if (!process_write) {
        do_write();
    }
}

void Client::do_connect(const boost::asio::ip::tcp::resolver::results_type& eps) {
    boost::asio::async_connect(sock, eps,
        [this](boost::system::error_code ec, boost::asio::ip::tcp::endpoint) {
           if (!ec) {
               send_input_request(input_request);
           }
    });
}

void Client::send_input_request(input_req_ptr request) {
    boost::system::error_code error_code;

    boost::asio::write(sock, boost::asio::buffer(input_request->get_data(), input_request->get_length_request()), error_code);
    if (error_code) {
        sock.close();
        std::cout << "error when send login" << std::endl;
        return ;
    }
    else {
        std::cout << "send input_request: OK" << std::endl;
    }
    std::cout << "send: " << input_request->get_protocol_version() << " " << input_request->get_type_data() << " "
              << input_request->get_login() << input_request->get_password()
              << " " << input_request->get_length_request() << std::endl;
    return ;

    int32_t id;
    boost::asio::read(sock, boost::asio::buffer(&id, 4), error_code);
    if (error_code) {
        sock.close();
        std::cout << "error when read login-id" << std::endl;
        return ;
    }
    set_login_id(id);

    do_read_header();
}

void Client::do_read_header() {
    boost::asio::async_read(sock, boost::asio::buffer(receiving_message.get_buf_data(), Message::header_size),
        [this](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                if (receiving_message.decode_header())
                {
                    auto size_body = receiving_message.get_body_length();
                    do_read_body(size_body);
                }
            }
            else {
                sock.close();
            }
    });
}

void Client::do_read_body(std::size_t size_body) {
    boost::asio::async_read(sock, boost::asio::buffer(receiving_message.get_buf_id_login(), Message::Settings_zone + size_body),
        [this](boost::system::error_code error, std::size_t) {
            if (!error) {
                *(receiving_message.get_buf_body()+receiving_message.get_body_length()) = '\0';
                std::cout << receiving_message.get_buf_str_login() << ": "
                          << receiving_message.get_buf_body() << std::endl;
                do_read_header();
            }
            else {
                sock.close();
            }
    });
}

void Client::do_write() {
    boost::asio::async_write(sock, boost::asio::buffer(sending_message.front().get_buf_data(), sending_message.front().get_mes_length()),
        [this](boost::system::error_code ec, std::size_t) {
        if (!ec) {
//            std::cout << sending_message.front() << std::endl;
//            std::cout << "mes with room_id=" << sending_message.front().get_room_id() << std::endl;
            sending_message.pop_front();
            if (!sending_message.empty()) do_write();
        }
        else {
            sock.close();
        }
    });
}

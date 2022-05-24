#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind/bind.hpp>
#include <boost/array.hpp>
#include <boost/program_options.hpp>

size_t max_length = 65536;

enum class Direction 
{
    Up,
    Right,
    Down,
    Left
};

enum class InputMessage 
{
    PlaceBomb,
    PlaceBlock,
    Move //{ direction: Direction },
};

void handle_message_from_server(char data[], std::size_t& length)
{
    fprintf(stderr, "tcp dostalismy %d bytow\n", length);
    std::cerr << data << std::endl;
    // tu czytamy co nam wysłał serwer 
    // zastanawiamy się nad tym 
    // i wklejamy do data odpowiedź jak będziemy chcieli żeby ją wysłać to zmieniamy length i data i kończymy funkcję
    // chyba czekamy na gui też tutaj 
    // tu będzie aktywne czekanie na gui
}

void handle_message_from_gui(char data[], std::size_t& length)
{
    fprintf(stderr, "udp dostalismy %d bytow\n", length);
    std::cerr << data << std::endl;
    // tu czytamy co nam wysłało gui 
    // zastanawiamy się nad tym 
    // i wklejamy do data odpowiedź jak będziemy chcieli żeby ją wysłać to zmieniamy length i data i kończymy funkcję
    // aktywne czekanie na serwer
}

void udp_server(boost::asio::io_context& io_context, std::string address, unsigned short port_listening, unsigned short port_sending)
{
    fprintf(stderr, "witamy w serwerze udp\n");
    boost::asio::ip::udp::udp::resolver resolver_listening(io_context);
    boost::asio::ip::udp::udp::resolver resolver_sending(io_context);
    boost::asio::ip::udp::udp::endpoint endpoint_listening = *(resolver_listening.resolve(address, std::to_string(port_listening)));
    boost::asio::ip::udp::udp::endpoint endpoint_sending = *(resolver_sending.resolve(address, std::to_string(port_sending)));
    boost::asio::ip::udp::udp::socket sock_listening(io_context, endpoint_listening);
    boost::asio::ip::udp::udp::socket sock_sending(io_context, endpoint_sending);
    /*boost::asio::ip::udp::udp::socket sock_listening(io_context, boost::asio::ip::udp::udp::endpoint(boost::asio::ip::udp::udp::v6(), port_listening));
    boost::asio::ip::udp::udp::socket sock_sending(io_context, boost::asio::ip::udp::udp::endpoint(boost::asio::ip::udp::udp::v6(), port_sending));*/
    for (;;)
    {
        char data[max_length];
        boost::asio::ip::udp::udp::endpoint sender_endpoint;
        size_t length = sock_listening.receive_from(
            boost::asio::buffer(data, max_length), sender_endpoint);
        handle_message_from_gui(data, length);
        sock_sending.send_to(boost::asio::buffer(data, length), sender_endpoint);
    }
}

void session(boost::asio::ip::tcp::tcp::socket sock)
{
    fprintf(stderr, "witamy w sesji tcp\n");
    try
    {
        sock.set_option(boost::asio::ip::tcp::tcp::no_delay(true));
        for (;;)
        {
            char data[max_length];

            boost::system::error_code error;
            size_t length = sock.read_some(boost::asio::buffer(data, max_length), error);
            if (error == boost::asio::error::eof)
                break; // Connection closed cleanly by peer.
            else if (error)
                throw boost::system::system_error(error); // Some other error.

            handle_message_from_server(data, length);
            boost::asio::write(sock, boost::asio::buffer(data, length));
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception in thread: " << e.what() << "\n";
    }
}

void tcp_server(boost::asio::io_context& io_context, std::string address, unsigned short port)
{
    fprintf(stderr, "witamy w serwerze tcp\n");
    boost::asio::ip::tcp::tcp::resolver resolver(io_context);
    boost::asio::ip::tcp::tcp::endpoint endpoint = *(resolver.resolve(address, std::to_string(port)));
    //boost::asio::ip::tcp::tcp::acceptor a(io_context, boost::asio::ip::tcp::tcp::endpoint(boost::asio::ip::tcp::tcp::v6(), port));
    boost::asio::ip::tcp::tcp::acceptor a(io_context, boost::asio::ip::tcp::tcp::endpoint(endpoint));
    for (;;)
    {
        std::thread(session, a.accept()).detach();
    }
}

void debug_1() {}

int main(int argc, char *argv[])
{
    std::string player_name;
    boost::program_options::options_description description("App usage");
    boost::program_options::variables_map parameter_map;
    uint16_t port_listening_gui;
    uint16_t port_sending_gui;
    uint16_t port_server;
    std::string gui_host_address;
    std::string server_host_address;
     
    description.add_options()
        ("help,h", "Get help")
        ("gui-address,d", boost::program_options::value<std::string>(), "Gui address")
        ("player-name,n", boost::program_options::value<std::string>(), "Player name")
        ("port,p", boost::program_options::value<uint16_t>(), "Port for listening gui")
        ("server-address,s", boost::program_options::value<std::string>(), "Server address");

        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(description).run(), parameter_map);
    boost::program_options::notify(parameter_map);

    if (parameter_map.count("help"))
    {
        std::cout << description << std::endl;
        return 0;
    }

    if (parameter_map.count("gui-address"))
    {
        std::string gui_address = parameter_map["gui-address"].as<std::string>();
        size_t last_colon_pos = gui_address.rfind(':');
        std::string host_part = gui_address.substr(0, last_colon_pos);
        std::string port_part = gui_address.substr(last_colon_pos + 1);
        port_sending_gui = (uint16_t)std::stoi(port_part);
        gui_host_address = host_part;
    }
    else
    {
        std::cout << "No gui address received. Terminating.\n";
        return 0;
    }

    if (parameter_map.count("player-name"))
    {
        player_name = parameter_map["player-name"].as<std::string>();
    }
    else
    {
        std::cout << "No player name received. Terminating.\n";
        return 0;
    }

    if (parameter_map.count("port"))
    {
        port_server = parameter_map["port"].as<uint16_t>();
    }
    else
    {
        std::cout << "No port received. Terminating.\n";
        return 0;
    }

    if (parameter_map.count("server-address"))
    {
        std::string server_address = parameter_map["server-address"].as<std::string>();
        size_t last_colon_pos = server_address.rfind(':');
        std::string host_part = server_address.substr(0, last_colon_pos);
        std::string port_part = server_address.substr(last_colon_pos + 1);
        port_server = (uint16_t)std::stoi(port_part);
        server_host_address = host_part;
    }
    else
    {
        std::cout << "No server address received. Terminating.\n";
        return 0;
    }
   
    try
    {
        //boost::asio::io_context io_context_udp;
        boost::asio::io_context io_context_tcp;

        std::thread debug_2(debug_1);
        debug_2.detach();
        std::thread udp_thread([gui_host_address, port_listening_gui, port_sending_gui]()
            {
                boost::asio::io_context udp_io_context;
                udp_server(udp_io_context, gui_host_address, port_listening_gui, port_sending_gui);
            });
        udp_thread.detach();

        //std::thread udp_thread(udp_server, io_context_udp, port_listening_gui, port_sending_gui);

        std::thread tcp_thread([port_server, server_host_address]() 
            {
                boost::asio::io_context io_context_tcp;
                tcp_server(io_context_tcp, server_host_address, port_server); 
            });
        tcp_thread.detach();

        //std::thread tcp_thread(tcp_server, io_context_tcp, port_server);
    }
    catch (std::exception& e)
    {
        fprintf(stderr, "wypisujemy wyjatek\n");
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

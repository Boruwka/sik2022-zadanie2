#include <iostream>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind/bind.hpp>
#include <boost/array.hpp>
#include <boost/program_options.hpp>

enum Direction

enum InputMessage 
{
    PlaceBomb,
    PlaceBlock,
    Move { direction: Direction },
}

void handle_message_from_server(char data_[], std::size_t& length)
{
    // tu czytamy co nam wysłał serwer 
    // zastanawiamy się nad tym 
    // i wklejamy do data odpowiedź jak będziemy chcieli żeby ją wysłać to zmieniamy length i data i kończymy funkcję
    // chyba czekamy na gui też tutaj 
    // tu będzie coś asynchronicznego
}

void handle_message_from_gui(char data_[], std::size_t& length)
{
    // tu czytamy co nam wysłało gui 
    // zastanawiamy się nad tym 
    // i wklejamy do data odpowiedź jak będziemy chcieli żeby ją wysłać to zmieniamy length i data i kończymy funkcję
    // asynchroniczne czekanie na serwer
}

class udp_server
{
public:
    udp_server(boost::asio::io_context& io_context, short port)
        : io_context_(io_context),
        socket_(io_context,             boost::asio::ip::udp::udp::endpoint(boost::asio::ip::udp::udp::v6(), port))
    {
        fprintf(stderr, "witamy w konstruktorze\n");
        socket_.async_receive_from(
        boost::asio::buffer(data_, max_length), sender_endpoint_,
        boost::bind(&udp_server::handle_receive_from, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
    }

    void handle_receive_from(const boost::system::error_code& error,
        size_t bytes_recvd)
    {
        fprintf(stderr, "dostalem cos z udp %d bytow\n", bytes_recvd);
        std::cerr << data_ << std::endl;
        if (!error && bytes_recvd > 0)
        {
            socket_.async_send_to(
            boost::asio::buffer(data_, bytes_recvd), sender_endpoint_,
            boost::bind(&udp_server::handle_send_to, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
        }
        else
        {
            socket_.async_receive_from(
            boost::asio::buffer(data_, max_length), sender_endpoint_,
            boost::bind(&udp_server::handle_receive_from, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
        }
    }

    void handle_send_to(const boost::system::error_code& error, size_t bytes_sent)
    {
        socket_.async_receive_from(
        boost::asio::buffer(data_, max_length), sender_endpoint_,
        boost::bind(&udp_server::handle_receive_from, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
    }

private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::udp::udp::socket socket_;
    boost::asio::ip::udp::udp::endpoint sender_endpoint_;
    enum { max_length = 1024 };
    char data_[max_length];
};

class session
    : public std::enable_shared_from_this<session>
{
public:
    session(boost::asio::ip::tcp::tcp::socket socket)
        : socket_(std::move(socket))
    {
    }

    void start()
    {
        do_read();
    }

private:
    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length)
        {
            fprintf(stderr, "dostalem cos z tcp, %d bajtow\n", length);
            if (!ec)
            {
                handle_message_from_server(data_, length);
                do_write(length);
            }
        });
    }

    void do_write(std::size_t length)
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
            [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                do_read();
            }
        });
    }

    boost::asio::ip::tcp::tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
};

class tcp_server
{
public:
  tcp_server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, boost::asio::ip::tcp::tcp::endpoint(boost::asio::ip::tcp::tcp::v6(), port))
  {
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          do_accept();
        });
  }

  boost::asio::ip::tcp::acceptor acceptor_;
};

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
        ("playner-name,n", boost::program_options::value<std::string>(), "Player name")
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
        std::string gui_address = parameter_map["gui-address"];
        last_colon_pos = gui_address.rfind(':');
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
        player_name = parameter_map["player-name"];
    }
    else
    {
        std::cout << "No player name received. Terminating.\n";
        return 0;
    }

    if (parameter_map.count("port"))
    {
        port = parameter_map["port"];
    }
    else
    {
        std::cout << "No port received. Terminating.\n";
        return 0;
    }

    if (parameter_map.count("server-address"))
    {
        std::string server_address = parameter_map["server-address"];
        last_colon_pos = gui_address.rfind(':');
        std::string host_part = gui_address.substr(0, last_colon_pos);
        std::string port_part = gui_address.substr(last_colon_pos + 1);
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
        boost::asio::io_context io_context;
        udp_server server1(io_context, serverPort1);
        tcp_server server2(io_context, serverPort2);
        io_context.run();
    }
    catch (std::exception& e)
    {
        fprintf(stderr, "wypisujemy wyjatek\n");
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

#include <iostream>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind/bind.hpp>
#include <boost/array.hpp>

std::string global_message = "aaa";
int serverPort1 = 11235;
int serverPort2 = 11236;

class udp_server
{
public:
  udp_server(boost::asio::io_context& io_context, short port)
    : io_context_(io_context),
      socket_(io_context, boost::asio::ip::udp::udp::endpoint(boost::asio::ip::udp::udp::v6(), port))
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

int main()
{
  try
  {
    fprintf(stderr, "tu dziala\n");
    boost::asio::io_context io_context;
    udp_server server1(io_context, serverPort1);    
    fprintf(stderr, "tu dziala 2\n");
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

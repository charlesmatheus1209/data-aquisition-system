#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <fstream>


using boost::asio::ip::tcp;

struct record_t
{
	 int  id;
	 char name[16];
	 char phone[16]; // 3134679819
};

class file {

    public:
    std::string filename = "";
    
    file::file(std::string filename){
        this->filename = filename;
    }

    void file::append_in_file(){
        std::fstream file(filename, std::fstream::out | std::fstream::in | std::fstream::binary 
																	 | std::fstream::app); 

        if (file.is_open()){
            std::cout << "Arquivo sendo escrito...." << std::endl;
        }else{
            std::cout << "Error opening file!" << std::endl;
        }                                                             
    }

    void file::read_file(){
        std::cout << "Arquivo sendo lido...." << std::endl;
    }
};


class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
  }

  void start()
  {
    read_message();
  }

private:
  void read_message()
  {
    auto self(shared_from_this());
    boost::asio::async_read_until(socket_, buffer_, "\r\n",
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            std::cout << "---------------------------------" << std::endl;
            std::istream is(&buffer_);
            std::string message(std::istreambuf_iterator<char>(is), {});
            std::cout << "Received: " << message << std::endl;

            if(message.size() >= 3){
                std::string type = message.substr(0, 3);
                if(type == "GET"){
                    std::cout << "Essa mensagem é do tipo GET" << std::endl;
                    write_message("Foi um GET");
                }else if(type == "LOG"){
                    std::cout << "Essa mensagem é do tipo LOG" << std::endl;
                    write_message("Foi um LOG");
                }else{
                    std::cout << "A mensagem não foi entendida pelo servidor" << std::endl;
                    write_message("ERROR|INVALID REQUEST\r\n");
                }
                

            }else{
                write_message("ERROR|INVALID REQUEST\r\n");
            }
          }
        });
  }

  void write_message(const std::string& message)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(message),
        [this, self, message](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            read_message();
          }
        });
  }

  tcp::socket socket_;
  boost::asio::streambuf buffer_;
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    accept();
  }

private:
  void accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    std::cerr << "Usage: chat_server <port>\n";
    return 1;
  }

  boost::asio::io_context io_context;

  server s(io_context, std::atoi(argv[1]));

  io_context.run();

  return 0;
}

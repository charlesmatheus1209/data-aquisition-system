#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <fstream>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>

using boost::asio::ip::tcp;

struct LogRecord {
    char sensor_id[32]; // supondo um ID de sensor de até 32 caracteres
    std::time_t timestamp; // timestamp UNIX
    double value; // valor da leitura
};


std::time_t string_to_time_t(const std::string& time_string) {
    std::tm tm = {};
    std::istringstream ss(time_string);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm);
}

std::string time_t_to_string(std::time_t time) {
    std::tm* tm = std::localtime(&time);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    
    while (getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    
    return result;
}

class file {
public:
    std::string filename;

    // Construtor
    file(std::string filename) {
        this->filename = "../src/" + filename + ".dat";
    }

    // Método para append em arquivo
    bool append_in_file(LogRecord record) {
      std::fstream file(filename, std::fstream::out | std::fstream::in | std::fstream::binary | std::fstream::app); 
      if (file.is_open()) {
          std::cout << "Arquivo sendo escrito...." << std::endl;
          file.write((char*)&record, sizeof(record));
          file.close();
          return true;
      } else {
          std::cout << "Error opening file!" << std::endl;
          return false;
      }
    }

    // Método para leitura de arquivo
    std::string read_file(int number) {
      std::fstream file(filename, std::fstream::in | std::fstream::binary); 
      if (file.is_open()) {
          std::cout << "Arquivo sendo lido...." << std::endl;
          
          // Vai para o fim do arquivo para determinar o tamanho
          file.seekg(0, std::ios::end);
          int file_size = file.tellg();
          
          // Calcula o número de registros presentes no arquivo
          int n = file_size / sizeof(LogRecord);

          if (n == 0) {
              std::cout << "Arquivo vazio." << std::endl;
              file.close();
              return "";
          }

          int id = (n > number) ? n - number : 0;
          file.seekg(id * sizeof(LogRecord), std::ios_base::beg);

          int qtd = (n > number) ? number : n;
          std::string retorno = std::to_string(qtd) + ";";
          // Lê e imprime os registros selecionados
          for (int i = id; i < n; ++i) {
              LogRecord rec;
              file.read((char*)&rec, sizeof(LogRecord));
              std::cout << "Id: " << rec.sensor_id << " - time: " << time_t_to_string(rec.timestamp) << " - value: " << rec.value << std::endl;
              retorno += time_t_to_string(rec.timestamp) + "|" + std::to_string(rec.value) + ";";
          }
          retorno.erase(retorno.length() - 1);
          retorno += "\r\n";

          file.close();
          return retorno;
      } else {
          std::cout << "Arquivo não sendo lido...." << std::endl;
          return "";
      }
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
            
            std::vector<std::string> message_splited = splitString(message, '|');

            if(message_splited.size() >= 3){
                if(message_splited[0] == "GET"){
                    std::cout << "Essa mensagem é do tipo GET" << std::endl;                  

                    file f(message_splited[1]);
                    std::string retorno = f.read_file(stoi(message_splited[2]));

                    if(retorno != ""){
                      write_message(retorno);
                    }else{
                      write_message("ERROR|INVALID_SENSOR_ID\r\n");
                    }

                }else if(message_splited[0] == "LOG"){
                    std::cout << "Essa mensagem é do tipo LOG" << std::endl;
                    LogRecord record; 
                    std::strncpy(record.sensor_id, message_splited[1].c_str(), sizeof(record.sensor_id) - 1);
                    record.timestamp = string_to_time_t(message_splited[2]);
                    record.value = stod(message_splited[3]);

                    file f(message_splited[1]);
                    bool ret = f.append_in_file(record);
                    
                    read_message();
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
  /*if (argc != 2)
  {
    std::cerr << "Usage: chat_server <port>\n";
    return 1;
  }*/

  boost::asio::io_context io_context;

  server s(io_context, 9000/*std::atoi(argv[1])*/);

  io_context.run();

  return 0;
}

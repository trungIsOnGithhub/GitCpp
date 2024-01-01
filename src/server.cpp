#include<iostream>
#include<cstdlib>
#include<string>
#include<cstring>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netdb.h>

std::atomic<bool> running(true);

std::mutex threads_mutex;

std::vector<std::thread> threads;

std::string directory = "/default/directory/path";


bool fileExists(const std::string& path) {
  struct stat buffer;
  return (stat(path.c_str(), &buffer) == 0);
}

bool isSafePath(const std::string& base, const std::string& path) {
  // simple check
  return path.find(base) == 0;
}


bool readHTTPLine(std::istringstream& stream, std::string& line) {
  if (std::getline(stream, line, '\r')) {
    if (stream.peek() == '\n') {
      stream.get();
    }
    return true;
  }
  return false;
}

std::string getFileContent(std::string& full_path) {

  std::ifstream file(full_path);

  if (!file.is_open()) {

    return "Could not open file: " + full_path + "\n";

  }

  std::stringstream buffer;

  buffer << file.rdbuf();

  return buffer.str();

}

std::string buildStatus200Response(const std::string& content, const std::string& contentType) {
  std::ostringstream response_stream;

  response_stream << "HTTP/1.1 200 OK\r\n";
  response_stream << "Content-Type: " << contentType << "\r\n";
  response_stream << "Content-Length: " << content.length() << "\r\n";
  response_stream << "\r\n";
  response_stream << content;

  return response_stream.str();
}

std::string build201Response(const std::string content, const std::string contentType, const std::string full_path) {
  std::ostringstream response_stream;

  response_stream << "HTTP/1.1 201 Created\r\n";
  response_stream << "Location: " << full_path << "\r\n";
  response_stream << "Content-Type: " << contentType << "\r\n";
  response_stream << "Content-Length: " << content.length() << "\r\n";
  response_stream << "\r\n";

  return response_stream.str();
}

std::string buildStatus404Response() {
  std::ostringstream response_stream;

  response_stream << "HTTP/1.1 404 Not Found\r\n\r\n";

  return response_steram.str();
}

std::string buildStatus500Response() {
  std::ostringstream response_stream;

  response_stream << "HTTP/1.1 500 Internal Server Error\r\n";
  response_stream << "Content-Length: 0\r\n";

  return response_stream.str();
}

std::string post(std::string path, std::string host, std::string userAgent, std::string body) {

  std::string response;
  std::cout << "The path requested was: " << path << std::endl;
  std::string filename;

  if (path.find("/files/") == 0) {

    filename = path.substr(7);

    std::string full_path = directory + filename;
    std::cout << "The full path is: " << full_path << std::endl;
    std::ofstream file(full_path);

    if (file.is_open()) {

      file << body;
      file.close();

      return build201Response(body, "application/octet-stream", full_path);
    }

    return buildStatus500Response();
  } else {
    std::cout << "Operation not supported\n";
    return buildStatus500Response();
  }
}


std::string get(std::string path, std::string host, std::string userAgent) {

  std::string response;
  std::cout << "The path requested was: " << path << std::endl;

  if (path == "/") {
      response = "HTTP/1.1 200 OK \r\n\r\n";
  } else {
      std::ostringstream response_stream;

      if (path.find("/echo/") == 0) {

        std::cout << "The GET request was an echo request\n";

        std::string statement = path.substr(6);

        return buildStatus200Response(statement, "text/plain");
        // return HttpResponseFactory::createResponse(200, statement, "text/plain").getResponseString();
      }
      else if (path.find("/user-agent") == 0) {
        std::cout << "The GET request was a user-agent request\n";

        std::string statement = path.substr(11);
        std::string agent = userAgent.substr(12);

        return buildStatus200Response(agent, "text/plain");
      }
      else if (path.find("/files/") == 0) {
        std::cout << "The GET request was a files request\n";

        std::string file_path = path.substr(7);

        std::string full_path = directory + "/" + file_path;

        std::cout << "Looking for file: " << file_path << ".\nIn directory: " << directory << std::endl;

        if (isSafePath(directory, full_path)) {

          if (fileExists(full_path)) {

            std::cout << "File exists: " << full_path << std::endl;

            return buildStatus200Response(getFileContent(full_path), "application/octet-stream");
          } else {

            std::cout << "File does not exist: " << full_path << std::endl;

            return buildStatus404Response();
          }

        } else {}
        } else {
          return buildStatus404Response();
          // return HttpResponseFactory::createResponse(404).getResponseString();
        }

        std::cout << "The GET request was not yet supported\n";
        // return HttpResponseFactory::createResponse(404).getResponseString();
        return buildStatus404Response();

      }

  return response;
}


void handleClient(int client_socket_fd) {
  const ssize_t buffer_size = 500;
  char buffer[buffer_size];

  memset(buffer, 0, buffer_size);

  // recv - receive a message from a connected socket
  ssize_t message_size = recv(client_socket_fd, &buffer, buffer_size, 0);

  if (message_size == -1){
    std::cout << "There was an error receiving the message\n";
    close(client_socket_fd);
    return;
  }

  // NUll termination
  buffer[message_size] = '\0';

  std::cout << "Got message: " << buffer << std::endl;
  // Get the first line of the request

  std::string request_string(buffer);

  std::istringstream request_stream(request_string);

  std::string start_line, middle_line, end_line;

  if (readHTTPLine(request_stream, start_line) &&
        readHTTPLine(request_stream, middle_line) && readHTTPLine(request_stream, end_line)) {
        std::cout << "Parsed HTTP Request\n";
  }
  else {
    std::cout << "Error reading HTTP Request\n";
  }

  std::string method, path, version;
  std::istringstream start_line_stream(start_line);
  // Parse the first line of the request
  // Handle the first line of the request
  start_line_stream >> method >> path >> version;

  std::string response;
  ssize_t send_size;

  if (method == "GET") {
    std::cout << "Handling a GET request\n";
    response = get(path, middle_line, end_line);
  }
  else if (method == "POST") {
    std::cout << "Handling a POST request\n";
    std::string contentLength, encoding, emptyString, body;

    if ( readHTTPLine(request_stream, contentLength) && readHTTPLine(request_stream, encoding) &&
          readHTTPLine(request_stream, emptyString) && readHTTPLine(request_stream, body)) {

        std::cout << "Parsed the rest of the message" << std::endl;
        response = post(path, middle_line, end_line, body);
      }
  }

  std::cout << "Sending message: " << response << std::endl;
  send_size = send(client_socket_fd, response.c_str(), response.size(), 0);

  if (send_size == -1) {
    std::cout << "There was an error sending a response\n";
  } else {
    std::cout << "Successfully sent response\n";
  } 

  close(client_socket_fd);
}


void addThread(std::thread&& new_thread) {
  std::lock_guard<std::mutex> guard(threads_mutex);
  threads.push_back(std::move(new_thread));
}

int main(int argc, char **argv) {
  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";

  int opt;
  int option_index;

  static struct option long_options[] = {
    {"directory", required_argument, 0, 'd'},
    {0,0,0,0}
  };

  while (
    (opt = getopt_long(argc, argv, "d:", long_options, &option_index)) != -1 ) {
      switch (opt) {
        case 'd':
          directory = optarg;
          break;
        case '?':
          return 1;
        default:
          break;
      }
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }

  // Since the tester restarts your program quite often, setting REUSE_PORT
  // ensures that we don't run into 'Address already in use' errors
  // int reuse = 1;
  // if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
  //   std::cerr << "setsockopt failed\n";
  //   return 1;
  // }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  std::cout<<"Waiting for a client to connect...\n";

  while(running) {
    int client_socket_fd = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t *)&client_addr_len);

    if (client_socket_fd == -1) {
      if (running) {
        std::cout << "Error accepting connection\n";
      }
      continue;
    }

    std::cout << "Client connected\n";
    std::thread clientThread(handleClient, client_socket_fd);

    addThread(std::move(clientThread));
  }

  // cleanup();

  // accept(server_fd, (struct sockaddr*) &client_addr, (socklen_t *) &client_addr_len);
  // std::cout<<"Client connected\n";

  close(server_fd);

  return 0;
}

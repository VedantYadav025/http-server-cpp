#include <arpa/inet.h>
#include <bits/types/FILE.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sys/stat.h>
#include <ios>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <type_traits>
#include <unistd.h>

#include <sstream>

std::string getResponse(const std::string &request) {
  if (request.empty()) {
    return "HTTP/1.1 400 Bad Request\r\n\r\n";
  }

  size_t method_end = request.find(' ');
  if (method_end == std::string::npos)
    return "HTTP/1.1 400 Bad Request\r\n\r\n";

  size_t path_end = request.find(' ', method_end + 1);
  if (path_end == std::string::npos)
    return "HTTP/1.1 400 Bad Request\r\n\r\n";

  std::string path = request.substr(method_end + 1, path_end - method_end - 1);

  std::istringstream stream(request);
  std::string line;
  std::getline(stream, line); // Skip request line

  std::string user_agent;

  while (std::getline(stream, line) && line != "\r" && !line.empty()) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    const std::string ua_header = "User-Agent: ";
    if (line.compare(0, ua_header.size(), ua_header) == 0) {
      user_agent = line.substr(ua_header.size());
    }
  }

  if (path == "/") {
    return "HTTP/1.1 200 OK\r\n\r\n";
  }

  if (path == "/user-agent") {
    std::string body =
        user_agent.empty() ? "User-Agent header not found" : user_agent;

    std::string headers = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: " +
                          std::to_string(body.size()) + "\r\n\r\n";

    return headers + body;
  }

  const std::string echo_prefix = "/echo/";
  if (path.rfind(echo_prefix, 0) == 0) {
    std::string to_echo = path.substr(echo_prefix.length());
    std::string body = to_echo;
    if (!user_agent.empty()) {
      body += "\nUser-Agent: " + user_agent;
    }
    std::string headers = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: " +
                          std::to_string(body.size()) + "\r\n\r\n";
    return headers + body;
  }

  return "HTTP/1.1 404 Not Found\r\n\r\n";
}

void respondToRequest(int client_fd) {
  char buffer[4096];
  if (recv(client_fd, buffer, sizeof(buffer) - 1, 0) == -1) {
    std::cout << "Failed to read data\n";
    close(client_fd);
    return;
  }
  std::string response = getResponse(std::string(buffer));
  send(client_fd, response.c_str(), response.size(), 0);
  close(client_fd);
  std::cout << "Client disconnected\n";
}

std::string getBody(const std::string &request) {
  const std::string delimiter = "\r\n\r\n";
  size_t pos = request.find(delimiter);
  if (pos == std::string::npos) {
    // No body present
    return "";
  }
  return request.substr(pos + delimiter.length());
}

std::string getRequestedFileName(const std::string &request) {
  // Find the request path
  size_t method_end = request.find(' ');
  if (method_end == std::string::npos)
    return "";

  size_t path_end = request.find(' ', method_end + 1);
  if (path_end == std::string::npos)
    return "";

  std::string path = request.substr(method_end + 1, path_end - method_end - 1);

  const std::string prefix = "/files/";
  if (path.rfind(prefix, 0) != 0)
    return "";

  return path.substr(prefix.size()); // Extract filename from path
}

std::string getHandle(int client_fd, std::string directory, const char* request) {

  if (std::string(request).find("files") == std::string::npos) {
    return getResponse(std::string(request));
  }

  std::string file_path = directory + getRequestedFileName(request);
  std::cout << "File path is " << file_path << std::endl;
  FILE* fptr = fopen(file_path.c_str(), "r");
  if (fptr == NULL) {
    return "HTTP/1.1 404 Not Found\r\n\r\n";
  }

  char myString[4000];
  fgets(myString, 4000, fptr);
  std::string s(myString);

  std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: ";
  response += std::to_string(s.size());
  response += "\r\n\r\n";
  response += myString;

  return response;
}

void handleFilePost(int client_fd, std::string directory) {
	char buffer[4096];
  if (recv(client_fd, buffer, sizeof(buffer) - 1, 0) < 0) {
    std::cout << "Failed to read data\n";
    close(client_fd);
    return;
  }

  if (buffer[0] == 'G') {
    std::cout << "here\n";
    std::string response = getHandle(client_fd, directory, buffer);
    send(client_fd, response.c_str(), response.size(), 0);
    return;
  }

  std::cout << "here\n";

  std::cout << directory << std::endl;
  std::string body = getBody(std::string(buffer));
  std::string file_name = getRequestedFileName(std::string(buffer));


  std::string file_path = directory + "/" + file_name;

  FILE* fptr = fopen(file_path.c_str(), "w");
  fprintf(fptr, body.c_str());
  fclose(fptr);

  std::string response = "HTTP/1.1 201 Created\r\n\r\n";
  send(client_fd, response.c_str(), response.size(), 0);
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::string directory;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--directory" && i + 1 < argc) {
      directory = argv[i + 1];
    }
  }

  // You can use print statements as follows for debugging, they'll be visible
  // when running tests.
  std::cout << "Logs from your program will appear here!\n";

  // Uncomment this block to pass the first stage
  //
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) !=
      0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  std::cout << "Waiting for a client to connect...\n";

  // ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);

  // // memset(buffer, 0, sizeof(buffer));

  // std::string response = getResponseForUserAgent(buffer);

  // std::cout << "response sent is " << response << std::endl;

  // send(client_fd, response.data(), strlen(response.data()), 0);

  while (true) {

    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                           (socklen_t *)&client_addr_len);

    if (client_fd < 0) {
      std::cout << "Failed to connect to client\n";
      continue;
    }
    std::cout << "Client connected\n";
    std::thread(handleFilePost, client_fd, directory).detach();
  }

  close(server_fd);

  return 0;
}

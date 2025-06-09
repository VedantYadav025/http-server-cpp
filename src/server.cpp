#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

std::string getResponse(const char *request) {
  std::string str_request(request); // working correctly
  std::string echo;
  std::string response;

  // find if echo/ is present in the request
  int index_of_echo = str_request.find("echo");

  if (index_of_echo == std::string::npos) {

    int index_of_slash = str_request.find("/");
    if (str_request[index_of_slash + 1] == ' ') {
      return "HTTP/1.1 200 OK\r\n\r\n";
    }

    response = "HTTP/1.1 404 Not Found\r\n\r\n";
    return response;
  }

  index_of_echo += 5;

  int i = index_of_echo;
  while (str_request[i] != ' ') {
    echo.push_back(str_request[i]);
    i++;
  }
  response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ";
  response += std::to_string(echo.size()) + "\r\n\r\n" + echo;
  // std::cout << echo << std::endl;

  return response;
}

std::string getResponseForUserAgent(const char *request) {
  std::string str_request(request);
  int index_of_usr_agent = str_request.find("User-Agent");

  if (index_of_usr_agent == std::string::npos)
    return getResponse(request);

  index_of_usr_agent += 12;
  std::string usr_agent_response;
  int i = index_of_usr_agent;
  while (str_request[i] != '\r') {
    usr_agent_response.push_back(str_request[i]);
    i++;
  }
  std::string response =
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " +
      std::to_string(usr_agent_response.size()) + "\r\n\r\n" +
      usr_agent_response;

  return response;
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

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

  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);

  std::cout << "Waiting for a client to connect...\n";

  int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                         (socklen_t *)&client_addr_len);

  if (client_fd < 0) {
    std::cerr << "Failed to connect to client\n";
    return 1;
  }

  std::cout << "Client connected" << std::endl;

  char buffer[4096];
  ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);

  // memset(buffer, 0, sizeof(buffer));

  std::string response = getResponseForUserAgent(buffer);

  std::cout << "response sent is " << response << std::endl;

  send(client_fd, response.data(), strlen(response.data()), 0);

  close(client_fd);
  close(server_fd);

  return 0;
}

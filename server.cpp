#include "player.hpp"
#include "utils.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <iostream>
#include <list>
#include <map>
#include <math.h>
#include <mutex>
#include <netinet/in.h>
#include <random>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <utility>

float Vector2Distance(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

std::mutex players_mutex;
std::map<int, Player> players;

std::mutex packets_mutex;
std::list<std::pair<int, std::string>> packets;

std::mutex clients_mutex;
std::map<int, std::pair<int, std::thread>> clients;

std::mutex running_mutex;
std::map<int, bool> is_running;

Vector2 random_spawn_position(bool already_locked = false) {
  const int MAX_ATTEMPTS = 20;  // attempts to find a good position
  const int MIN_DISTANCE = 100; // minimum distance between players
  const int MARGIN = 50;  // keep players away from world edges
  const int WORLD_WIDTH = 2000;  // fixed world size
  const int WORLD_HEIGHT = 2000;
  
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_real_distribution<float> x_dist(MARGIN, WORLD_WIDTH - MARGIN);
  static std::uniform_real_distribution<float> y_dist(MARGIN, WORLD_HEIGHT - MARGIN);
  
  if (!already_locked) {
    std::cout << "Attempting to get lock..." << std::endl;
    std::unique_lock<std::mutex> lock(players_mutex, std::try_to_lock);
    if (!lock.owns_lock()) {
      std::cout << "Failed to get lock" << std::endl;
      return Vector2{(float)WORLD_WIDTH/2, (float)WORLD_HEIGHT/2};
    }
    std::cout << "Got lock, checking players..." << std::endl;
  }
  
  std::cout << "players.empty() = " << players.empty() << std::endl;
  
  if (players.empty()) {
    std::cout << "No players, returning random position" << std::endl;
    return Vector2{x_dist(gen), y_dist(gen)};
  }
  
  for (int i = 0; i < MAX_ATTEMPTS; i++) {
    float x = x_dist(gen);
    float y = y_dist(gen);
    bool too_close = false;
    
    for (const auto &[_, v] : players) {
      if (Vector2Distance(Vector2{x, y}, Vector2{(float)v.x, (float)v.y}) < MIN_DISTANCE) {
        too_close = true;
        break;
      }
    }
    
    if (!too_close) {
      std::cout << "Found good position" << std::endl;
      return Vector2{x, y};
    }
  }
  
  std::cout << "No good position found, using center" << std::endl;
  return Vector2{(float)WORLD_WIDTH/2, (float)WORLD_HEIGHT/2};
}

void handle_client(int client, int id) {
  {
    // default values...
    {
      std::lock_guard<std::mutex> lock(players_mutex);
      Vector2 spawn_pos = random_spawn_position(true);  // Pass true to indicate mutex is already locked
      std::cout << "Spawning player " << id << " at " << spawn_pos.x << ", " << spawn_pos.y << std::endl;
      Player p(spawn_pos.x, spawn_pos.y);
      p.username = "unset";
      p.color = RED;
      players.insert({id, p});
    }
    
    std::ostringstream out;
    {
      std::lock_guard<std::mutex> lock(players_mutex);
      for (const auto &[k, v] : players) {
        // sanitize 
        std::string safe_username = v.username;
        if (safe_username.empty()) safe_username = "unset";
        // remove bad characters
        for (char &c : safe_username) {
          if (c == ';' || c == ':' || c == ' ') c = '_';
        }
        // sanitize color
        unsigned int col = color_to_uint(v.color);
        if (col > 4) col = 1;
        out << ':' << k << ' ' << v.x << ' ' << v.y << ' ' << safe_username << ' ' << col;
      }
      std::cout << out.str() << std::endl;
    }
    
    std::string payload = out.str();
    std::string msg = "0\n" + payload;
    send_message(msg, client);
  }
  
  sleep(3);

  std::string msg_id = "1\n" + std::to_string(id);
  send_message(msg_id, client);

  std::ostringstream out;
  out << "3\n"
      << id << " " << players.at(id).x << " " << players.at(id).y << " "
      << players.at(id).username << " " << color_to_uint(players.at(id).color);

  for (const auto &pair : clients) {
    if (pair.first != id) {
      send_message(out.str(), pair.second.first);
    }
  }

  bool running;
  {
    std::lock_guard<std::mutex> _(running_mutex);
    running = is_running[id];
  }

  while (running) {
    char buffer[1024];
    int received = recv(client, buffer, sizeof(buffer), 0);

    if (received <= 0)
      break;
      
    {
      std::lock_guard<std::mutex> _(running_mutex);
      running = is_running[id];
    }

    if (!running)
      break;

    {
      std::lock_guard<std::mutex> lock(packets_mutex);
      packets.push_front({
          id,
          std::string(buffer, received),
      });
    }
  }

  {
    std::lock_guard<std::mutex> _(running_mutex);
    is_running[id] = false;
    std::lock_guard<std::mutex> _lock(players_mutex);
    players.erase(id);
  }

  std::cout << "Client " << id << " disconnected.\n";
}

void accept_clients(int sock) {
  while (true) {
    int client = accept(sock, nullptr, nullptr);
    int id = 0;
    while (1) {
      int id_init = id;
      for (auto &[k, v] : players)
        if (id == k)
          id++;

      if (id_init == id)
        break;
    }

    std::lock_guard<std::mutex> lock(clients_mutex);
    clients[id] = {client, std::thread(handle_client, client, id)};

    std::lock_guard<std::mutex> _(running_mutex);
    is_running[id] = true;
  }
}

int main() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("Failed to create socket");
    return -1;
  }

  sockaddr_in sock_addr;
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_port = htons(50000);
  sock_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
    perror("Failed to bind socket");
    return -1;
  }

  if (listen(sock, 5) < 0) {
    perror("Failed to listen on socket");
    return -1;
  }

  std::unordered_map<int, std::thread> client_threads;

  std::thread(accept_clients, sock).detach();

  std::cout << "Running.\n";

  // handle game stuff
  while (1)  { // loop until stop
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      std::lock_guard<std::mutex> lo(running_mutex);

      std::list<int> to_remove;
      for (auto &[id, v] : is_running) {
        if (!v) {
          to_remove.push_back(id);
        }
      }
      std::lock_guard<std::mutex> a(clients_mutex);
      for (int i : to_remove) {
        if (clients[i].second.joinable())
          clients[i].second.join();

        shutdown(clients[i].first, SHUT_RDWR);

        close(clients[i].first);
        is_running.erase(i);
        clients.erase(i);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::string out("4\n" + std::to_string(i));
        for (auto &pair : clients) {
          if (pair.first != i) {
            send_message(out, pair.second.first);
          } else {
          }
        }

        {
          std::lock_guard<std::mutex> b(players_mutex);
          players.erase(i);
        }
        std::cout << "Removed client " << i << std::endl;
      }
    }

    // ---------------------------------

    {
      std::lock_guard<std::mutex> lock(packets_mutex);

      for (auto &[from_id, packet] : packets) {

        int packet_type = std::stoi(packet.substr(0, packet.find('\n')));
        std::string payload = packet.substr(packet.find('\n') + 1);

        switch (packet_type) {
        case 2: {
          std::lock_guard<std::mutex> z(players_mutex);
          std::istringstream j(payload);
          int x, y;
          float rot;
          j >> x >> y >> rot;
          if (players.find(from_id) == players.end())
            break;
          players.at(from_id).x = x;
          players.at(from_id).y = y;
          players.at(from_id).rot = rot;
        }
          for (auto &pair : clients) {
            if (pair.first != from_id) {
              std::ostringstream out;
              out << "2\n" << from_id << " " << payload;
              send_message(out.str(), pair.second.first);
            }
          }
          break;

        case 5: {
          std::lock_guard<std::mutex> lock(players_mutex);
          players[from_id].username = payload;
          std::lock_guard<std::mutex> lokc(clients_mutex);
          for (auto &[k, v] : clients) {
            if (k == from_id)
              continue;
            send_message(std::string("5\n")
                             .append(std::to_string(from_id))
                             .append(" ")
                             .append(payload),
                         v.first);
          }
        } break;
        case 6: {
          // THIS PART IS GOOD
          std::lock_guard<std::mutex> lock(players_mutex);
          unsigned int x = (unsigned int)std::stoi(payload);
          std::cout << "Received code 6, setting player color to " << x
                    << std::endl;
          players[from_id].color = uint_to_color(x);
          std::cout << "Color is now " << color_to_uint(players[from_id].color)
                    << std::endl;
          std::lock_guard<std::mutex> lokc(clients_mutex);
          for (auto &[k, v] : clients) {
            if (k == from_id)
              continue;
            send_message(std::string("6\n")
                             .append(std::to_string(from_id))
                             .append(" ")
                             .append(payload),
                         v.first);
          }
        } break;

        case 10: {
          // bullet, 10\n<from_id> <rot>
          std::lock_guard<std::mutex> lock(players_mutex);
          std::istringstream j(payload);
          int rot;
          j >> rot;
          players[from_id].rot = rot;

          // broadcast bullet
          std::ostringstream out;
          out << "10\n" << from_id << " " << rot;
          broadcast_message(out.str(), clients); 
        }

        default:
          std::cerr << "INVALID PACKET TYPE: " << packet_type << std::endl;
          break;
        }
      }
      packets.clear();
    }
  }
  close(sock);
}

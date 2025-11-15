#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <print>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

static constexpr size_t stringLength(const char *str) {
  size_t count = 0;
  while (*str++)
    ++count;
  return count;
}

// Dynamic Variable resolve
// TODO(stanley): put into a namespace with free functions
class DynamicVariableResolver {
public:
  static std::string resolve(const std::string_view input) {
    static constexpr auto parenLength = stringLength("(");
    // removes $
    std::string_view varName = input.substr(1, input.length());
    auto paramStartPos = varName.find('(');
    bool hasParams = paramStartPos != std::string_view::npos;
    auto paramEndPos = varName.find(')');
    if (paramEndPos == std::string_view::npos && hasParams)
      return "";
    std::string_view prefix =
        varName.substr(0, hasParams ? paramStartPos : varName.size());

    std::string_view params = hasParams
                                  ? varName.substr(paramStartPos + parenLength,
                                                   paramEndPos - parenLength)
                                  : "";

    return generateVariable(prefix, params);
  }

private:
  static std::string generateVariable(const std::string_view variableType,
                                      const std::string_view params) {
    if (variableType == "uuid" || variableType == "random.uuid") {
      return generateUUID();
    } else if (variableType == "timestamp") {
      return generateTimestamp();
    } else if (variableType == "isoTimestamp") {
      return generateISOTimestamp();
    } else if (variableType == "randomInt" ||
               variableType == "random.integer") {
      return generateRandomInt(params);
    } else if (variableType == "random.float") {
      return generateRandomFloat(params);
    } else if (variableType == "random.alphabetic") {
      return generateRandomAlphabetic(params);
    } else if (variableType == "random.alphanumeric") {
      return generateRandomAlphanumeric(params);
    } else if (variableType == "random.hexadecimal") {
      return generateRandomHexadecimal(params);
    } else if (variableType == "random.email") {
      return generateRandomEmail();
    }

    return ""; // Unknown variable type
  }

  static std::string generateUUID() {
    // Generate a UUID v4
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    // Generate 32 hex digits
    std::string uuid;
    uuid.reserve(36); // 32 hex digits + 4 hyphens

    for (int i = 0; i < 32; i++) {
      if (i == 8 || i == 12 || i == 16 || i == 20) {
        uuid += '-';
      }

      if (i == 12) {
        uuid += '4'; // Version 4
      } else if (i == 16) {
        // Variant bits: 10xx
        uuid += static_cast<char>('8' + (dis(gen) % 4));
      } else {
        int value = dis(gen);
        uuid +=
            static_cast<char>(value < 10 ? '0' + value : 'a' + (value - 10));
      }
    }

    return uuid;
  }

  static std::string generateTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
            .count();
    return std::to_string(timestamp);
  }

  static std::string generateISOTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';

    return oss.str();
  }

  static std::string generateRandomInt(const std::string_view params) {
    std::random_device rd;
    std::mt19937 gen(rd());

    int from = 0, to = 1000;

    if (!params.empty()) {
      std::stringstream ss((std::string(params)));
      char comma;
      ss >> from >> comma >> to;
    }

    std::uniform_int_distribution<> dis(from, to - 1);
    return std::to_string(dis(gen));
  }

  static std::string generateRandomFloat(const std::string_view params) {
    std::random_device rd;
    std::mt19937 gen(rd());

    double from = 0.0, to = 1000.0;

    if (!params.empty()) {
      std::stringstream ss((std::string(params)));
      char comma;
      ss >> from >> comma >> to;
    }

    std::uniform_real_distribution<> dis(from, to);

    // Use std::format for C++20/23
    return std::format("{:.6f}", dis(gen));
  }

  static std::string generateRandomAlphabetic(const std::string_view params) {
    int length = 10;

    if (!params.empty()) {
      std::stringstream ss((std::string(params)));
      ss >> length;
    }

    if (length <= 0) {
      return "";
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 51); // 26 lowercase + 26 uppercase

    std::string result;
    result.reserve(length);

    for (int i = 0; i < length; i++) {
      int c = dis(gen);
      if (c < 26) {
        result += 'a' + c;
      } else {
        result += 'A' + (c - 26);
      }
    }

    return result;
  }

  static std::string generateRandomAlphanumeric(const std::string_view params) {
    int length = 10;

    if (!params.empty()) {
      std::stringstream ss((std::string(params)));
      ss >> length;
    }

    if (length <= 0) {
      return "";
    }

    std::random_device rd;
    std::mt19937 gen(rd());

    const std::string charset =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
    std::uniform_int_distribution<> dis(0, charset.size() - 1);

    std::string result;
    result.reserve(length);

    for (int i = 0; i < length; i++) {
      result += charset[dis(gen)];
    }

    return result;
  }

  static std::string generateRandomHexadecimal(const std::string_view params) {
    int length = 10;

    if (!params.empty()) {
      std::stringstream ss((std::string(params)));
      ss >> length;
    }

    if (length <= 0) {
      return "";
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::string result;
    result.reserve(length);

    for (int i = 0; i < length; i++) {
      int value = dis(gen);
      result +=
          static_cast<char>(value < 10 ? '0' + value : 'a' + (value - 10));
    }

    return result;
  }

  static std::string generateRandomEmail() {
    std::string username = generateRandomAlphabetic("8");
    std::string domain = generateRandomAlphabetic("6");
    std::string tld = generateRandomAlphabetic("3");

    return std::format("{}@{}.{}", username, domain, tld);
  }
};

// Forward declarations
class HttpRequest;
class RequestAdapter;
class CurlAdapter;

// HTTP Request structure
class HttpRequest {
public:
  std::string name;
  std::string method;
  std::string url;
  std::map<std::string, std::string> headers;
  std::string body;

  HttpRequest(const std::string &method, const std::string &url,
              const std::string &name = "")
      : method(method), url(url), name(name) {}

  void addHeader(const std::string &key, const std::string &value) {
    headers[key] = value;
  }

  void setBody(const std::string &body) { this->body = body; }
};

// Abstract adapter for request engines
class RequestAdapter {
public:
  virtual ~RequestAdapter() = default;
  virtual std::string executeRequest(const HttpRequest &request) = 0;
};

// cURL adapter implementation
class CurlAdapter : public RequestAdapter {
public:
  std::string executeRequest(const HttpRequest &request) override {
    std::string command =
        "curl -s -i -X " + request.method + " \"" + request.url + "\"";

    // Add headers
    for (const auto &header : request.headers) {
      command += " -H \"" + header.first + ": " + header.second + "\"";
    }

    // Add body if present
    if (!request.body.empty() &&
        (request.method == "POST" || request.method == "PUT" ||
         request.method == "PATCH")) {
      command += " -d '" + request.body + "'";
    }

    // Execute command and capture output
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) {
      return "Error executing request: Failed to open pipe.";
    }

    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      result += buffer;
    }

    int exitCode = pclose(pipe);
    if (exitCode != 0) {
      return "Error executing request: Command exited with code " +
             std::to_string(exitCode);
    }

    if (result.empty()) {
      return "No response received.";
    }

    return result;
  }
};

// Terminal menu for selecting requests
class RequestMenu {
private:
  std::vector<std::shared_ptr<HttpRequest>> requests;
  int selected = 0;
  bool showDetails = false;

public:
  void addRequest(std::shared_ptr<HttpRequest> request) {
    requests.push_back(request);
  }

  void display() {
    std::print("\033[2J\033[H");

    std::println("HTTP Request Selector");
    std::println("=====================\n");

    if (requests.empty()) {
      std::println("No requests available.");
      return;
    }

    if (showDetails && selected >= 0 && selected < requests.size()) {
      auto request = requests[selected];
      std::println("Name: {}", request->name);
      std::println("Method: {}", request->method);
      std::println("URL: {}", request->url);

      if (!request->headers.empty()) {
        std::println("Headers:");
        for (const auto &header : request->headers) {
          std::println("   {}: {}", header.first, header.second);
        }
      }

      if (!request->body.empty()) {
        std::println("Body:\n{}", request->body);
      }

      std::println("\nPress 'd' to toggle details, arrow keys to navigate, "
                   "Enter to select, q to quit.");
    } else {
      for (int i = 0; i < requests.size(); i++) {
        if (i == selected) {
          std::print("> ");
        } else {
          std::print("  ");
        }

        if (!requests[i]->name.empty()) {
          std::println("# {}", requests[i]->name);
          std::print("    ");
        }
        std::println("{} {}", requests[i]->method, requests[i]->url);
      }

      std::println("\nPress 'd' to toggle details, arrow keys to navigate, "
                   "Enter to select, q to quit.");
    }
  }

  void moveUp() {
    if (selected > 0) {
      selected--;
    }
  }

  void moveDown() {
    if (selected < requests.size() - 1) {
      selected++;
    }
  }

  void toggleDetails() { showDetails = !showDetails; }

  std::shared_ptr<HttpRequest> getSelected() {
    if (selected >= 0 && selected < requests.size()) {
      return requests[selected];
    }
    return nullptr;
  }

  void reset() {
    selected = 0;
    showDetails = false;
  }

  size_t size() const { return requests.size(); }
};

// HTTP Request Parser with variable support
class HttpRequestParser {
private:
  static std::map<std::string, std::string> variables;

  // Parse a variable declaration line
  static void parseVariable(const std::string &line) {
    // Remove leading @
    std::string varLine = line.substr(1);

    // Find the equal sign
    size_t equalPos = varLine.find('=');
    if (equalPos == std::string::npos) {
      return; // Invalid variable declaration
    }

    // Extract variable name and value
    std::string varName = varLine.substr(0, equalPos);
    std::string varValue = varLine.substr(equalPos + 1);

    // Trim whitespace
    varName.erase(0, varName.find_first_not_of(" \t"));
    varName.erase(varName.find_last_not_of(" \t") + 1);
    varValue.erase(0, varValue.find_first_not_of(" \t"));
    varValue.erase(varValue.find_last_not_of(" \t") + 1);

    // Handle quoted strings
    if (varValue.front() == '"' && varValue.back() == '"') {
      varValue = varValue.substr(1, varValue.length() - 2);
    }

    // Store the variable
    variables[varName] = varValue;
  }

  // Substitute variables in a string without using regex
  static std::string substituteVariables(const std::string &input) {
    std::string result = input;
    size_t pos = 0;

    while (pos < result.length()) {
      // Look for the start of a variable
      size_t start = result.find("{{", pos);
      if (start == std::string::npos) {
        break; // No more variables
      }

      // Look for the end of the variable
      size_t end = result.find("}}", start);
      if (end == std::string::npos) {
        break; // Malformed variable, stop processing
      }

      // Extract variable name
      std::string varName = result.substr(start + 2, end - start - 2);

      // Find the replacement value
      std::string replacement = varName.starts_with("$")
                                    ? DynamicVariableResolver::resolve(varName)
                                : variables.count(varName) ? variables[varName]
                                                           : "";

      // Replace the variable with its value
      result.replace(start, end - start + 2, replacement);

      // Update position to continue after the replacement
      pos = start + replacement.length();
    }

    return result;
  }

public:
  static std::vector<std::shared_ptr<HttpRequest>>
  parseFile(const std::string &filename) {
    std::vector<std::shared_ptr<HttpRequest>> requests;
    std::ifstream file(filename);

    if (!file.is_open()) {
      std::println(stderr, "Error: Could not open file {}", filename);
      return requests;
    }

    // Clear variables for a fresh parse
    variables.clear();

    std::string line;
    std::shared_ptr<HttpRequest> currentRequest = nullptr;
    bool inHeaders = false;
    bool inBody = false;
    std::string name;
    std::string body;

    while (std::getline(file, line)) {
      if (line.starts_with("# @name")) {
        constexpr auto nameSize = stringLength("# @name");
        name = std::string_view(line).substr(nameSize + 1);
        continue;
      }

      // Skip comments
      if (line.starts_with("#") || line.starts_with("//")) {
        continue;
      }

      // Parse variable declarations
      if (line.find("@") == 0) {
        parseVariable(line);
        continue;
      }

      // Skip empty lines
      if (line.empty()) {
        if (currentRequest && inHeaders) {
          inHeaders = false;
          inBody = true;
          body = "";
          name = "";
        }
        continue;
      }

      // Check if it's a new request (starts with HTTP method)
      if (line.find("GET ") == 0 || line.find("POST ") == 0 ||
          line.find("PUT ") == 0 || line.find("PATCH ") == 0 ||
          line.find("DELETE ") == 0) {

        // Save previous request if exists
        if (currentRequest) {
          if (inBody && !body.empty()) {
            currentRequest->setBody(body);
          }
          requests.push_back(currentRequest);
        }

        // Parse method and URL, substituting variables
        std::string processedLine = substituteVariables(line);
        size_t spacePos = processedLine.find(' ');
        std::string method = processedLine.substr(0, spacePos);
        std::string url = processedLine.substr(spacePos + 1);

        // Create new request
        currentRequest = std::make_shared<HttpRequest>(method, url, name);
        inHeaders = true;
        inBody = false;
        body = "";
        name = "";
      }
      // Parse headers
      else if (inHeaders && currentRequest) {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
          std::string key = line.substr(0, colonPos);
          std::string value = line.substr(colonPos + 1);

          // Trim whitespace
          key.erase(0, key.find_first_not_of(" \t"));
          key.erase(key.find_last_not_of(" \t") + 1);
          value.erase(0, value.find_first_not_of(" \t"));
          value.erase(value.find_last_not_of(" \t") + 1);

          // Substitute variables in header values
          value = substituteVariables(value);

          currentRequest->addHeader(key, value);
        }
      }
      // Parse body
      else if (inBody && currentRequest) {
        if (!body.empty()) {
          body += "\n";
        }
        body += line;
      }
    }

    // Add the last request if exists
    if (currentRequest) {
      if (inBody && !body.empty()) {
        // Substitute variables in body
        body = substituteVariables(body);
        currentRequest->setBody(body);
      }
      requests.push_back(currentRequest);
    }

    return requests;
  }
};

// Initialize the static member
std::map<std::string, std::string> HttpRequestParser::variables;

// Terminal input handler for arrow keys
class TerminalInput {
private:
  struct termios old_tio, new_tio;

public:
  TerminalInput() {
    // Get terminal settings
    tcgetattr(STDIN_FILENO, &old_tio);

    // Copy old settings to new settings
    new_tio = old_tio;

    // Disable canonical mode and echo
    new_tio.c_lflag &= ~(ICANON | ECHO);

    // Apply new settings
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
  }

  ~TerminalInput() {
    // Restore old settings
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
  }

  // Get a key press, handling special sequences like arrow keys
  int getKey() {
    int ch = getchar();

    // Check if it's an escape sequence (like arrow keys)
    if (ch == '\033') {
      // Check the next character
      if (getchar() == '[') {
        // Get the final character of the sequence
        switch (getchar()) {
        case 'A':
          return 1; // Up arrow
        case 'B':
          return 2; // Down arrow
        case 'C':
          return 3; // Right arrow
        case 'D':
          return 4; // Left arrow
        default:
          return 0; // Unknown sequence
        }
      }
    }

    return ch; // Regular character
  }
};

// Main application
class HttpRequestApp {
private:
  RequestMenu menu;
  std::unique_ptr<RequestAdapter> adapter;

public:
  HttpRequestApp() : adapter(std::make_unique<CurlAdapter>()) {}

  bool loadRequests(const std::string &filename) {
    auto requests = HttpRequestParser::parseFile(filename);
    if (requests.empty()) {
      std::println(stderr, "No valid requests found in file: {}", filename);
      return false;
    }

    for (const auto &request : requests) {
      menu.addRequest(request);
    }

    return true;
  }

  void run() {
    if (menu.size() == 0) {
      std::println("No requests to display. Exiting.");
      return;
    }

    TerminalInput input;

    while (true) {
      menu.display();

      int key = input.getKey();

      // Handle special keys
      if (key == 1) { // Up arrow
        menu.moveUp();
      } else if (key == 2) { // Down arrow
        menu.moveDown();
      } else if (key == 'q' || key == 'Q') {
        break;
      } else if (key == 'd' || key == 'D') {
        menu.toggleDetails();
      } else if (key == '\n') { // Enter key
        auto request = menu.getSelected();
        if (request) {
          std::println("\nExecuting request...");
          std::println("Method: {}", request->method);
          std::println("URL: {}", request->url);

          if (!request->headers.empty()) {
            std::println("Headers:");
            for (const auto &header : request->headers) {
              std::println("  {}: {}", header.first, header.second);
            }
          }

          if (!request->body.empty()) {
            std::println("Body:\n{}", request->body);
          }

          std::println("\nResponse:");
          std::string response = adapter->executeRequest(*request);
          std::println("{}\n", response);

          std::print("Press any key to continue...");
          input.getKey();
        }
      }
    }

    // Clear screen before exiting
    std::print("\033[2J\033[H");
  }
};

// Main function
int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::println(stderr, "Usage: {} <http_request_file>", argv[0]);
    return 1;
  }

  HttpRequestApp app;
  if (!app.loadRequests(argv[1])) {
    return 1;
  }

  app.run();

  return 0;
}

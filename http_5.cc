// TODO(stanley): normalize to use snake_case everywhere
// TODO(stanley): use free functions instead of classes
#include "MmapReader.hpp"
#include "TerminalInput.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <curl/curl.h>
#include <curl/easy.h>
#include <expected>
#include <format>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <print>
#include <random>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <termios.h>
#include <type_traits>
#include <unistd.h>
#include <vector>

enum class agatetepe_error { unknown, parse_error, curl_error };

struct error {
  agatetepe_error code = agatetepe_error::unknown;
  std::string message;
};

static constexpr size_t stringLength(const char *str) {
  size_t count = 0;
  while (*str++)
    ++count;
  return count;
}

// Dynamic Variable resolver based on Rider's dynamic variables behaviour:
// https://www.jetbrains.com/help/rider/HTTP-Client-variables.html#dynamic-variables
// which in turn is based on Java's Faker:
// https://javadoc.io/doc/com.github.javafaker/javafaker/latest/com/github/javafaker/package-summary.html
// TODO(stanley): put into a namespace with free functions
class DynamicVariableResolver {
public:
  static std::string resolve(const std::string_view input) {
    static constexpr auto parenLength = stringLength("(");
    // remove $
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
    // TODO(stanley): may use a map
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
    // TODO(stanley): maybe should be static and reuse
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
    // TODO(stanley): maybe should be static and reuse
    std::mt19937 gen(rd());

    double from = 0.0, to = 1000.0;

    if (!params.empty()) {
      std::stringstream ss((std::string(params)));
      char comma;
      ss >> from >> comma >> to;
    }

    std::uniform_real_distribution<> dis(from, to);

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

// Plain Old Data
struct HttpResponse {
  long statusCode = 0;
  std::optional<std::string> body;
  std::map<std::string, std::string> headers;
};

// HTTP Request structure
class HttpRequest {
public:
  std::string method;
  std::string url;
  std::string name;
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
  virtual std::expected<HttpResponse, error>
  doRequest(const HttpRequest &request) = 0;
};

// cURL adapter implementation
class CurlAdapter : public RequestAdapter {
public:
  CurlAdapter() {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
      throw std::runtime_error("Failed to initialise libcurl");
    }
  }

  ~CurlAdapter() override { curl_global_cleanup(); }

  std::expected<HttpResponse, error>
  doRequest(const HttpRequest &request) override {
    CURL *curl = curl_easy_init();
    if (!curl) {
      return std::unexpected(
          error{.code = agatetepe_error::curl_error,
                .message = "Failed to initialise cURL easy handler."});
    }

    std::string responseBody;
    std::map<std::string, std::string> responseHeaders;
    struct curl_slist *headersList = nullptr;

    // Set the URL
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);

    // --- Set HTTP Method and Body ---
    if (request.method == "POST") {
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    } else if (request.method == "PUT" || request.method == "PATCH" ||
               request.method == "DELETE") {
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());
      if (!request.body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
      }
    } else if (request.method != "GET") {
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());
    }

    // --- Set Headers ---
    for (const auto &header : request.headers) {
      std::string headerString = header.first + ": " + header.second;
      headersList = curl_slist_append(headersList, headerString.c_str());
    }

    if (headersList) {
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);
    }

    // Perform the request
    CURLcode res = curl_easy_perform(curl);

    // Check for transport errors (e.g., network failure, couldn't resolve host)
    if (res != CURLE_OK) {
      // Cleanup resources before returning error
      curl_easy_cleanup(curl);
      curl_slist_free_all(headersList);
      return std::unexpected(
          error{.code = agatetepe_error::curl_error,
                .message = std::format("curl_easy_perform() failed: {}",
                                       curl_easy_strerror(res))});
    }

    // Get the HTTP status code. This is now part of a successful transport.
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    // Cleanup resources
    curl_easy_cleanup(curl);
    curl_slist_free_all(headersList);

    // Construct and return the successful response object.
    // The caller is now responsible for checking the status code.
    HttpResponse response;
    response.statusCode = httpCode;
    response.body = responseBody;
    response.headers = std::move(responseHeaders);

    return response;
  }

private:
  static size_t writeCallback(void *contents, size_t size, size_t nmemb,
                              std::string *userp) {
    size_t totalSize = size * nmemb;
    if (userp) {
      userp->append((char *)contents, totalSize);
    }
    return totalSize;
  }

  static size_t headerCallback(char *buffer, size_t size, size_t nitems,
                               void *userdata) {
    auto *headers = static_cast<std::map<std::string, std::string> *>(userdata);
    size_t totalSize = size * nitems;

    std::string headerLine(buffer, totalSize);

    headerLine.erase(headerLine.find_last_not_of("\r\n") + 1);

    // Ignore empty lines and the HTTP status line (e.g., "HTTP/1.1 200 OK")
    if (headerLine.empty() || headerLine.find(':') == std::string::npos) {
      return totalSize;
    }

    size_t colonPosition = headerLine.find(':');
    std::string key = headerLine.substr(0, colonPosition);
    std::string value = headerLine.substr(colonPosition + 1);

    // Trim
    key.erase(0, key.find_first_not_of(' '));
    key.erase(key.find_last_not_of(' ') + 1);
    value.erase(0, value.find_first_not_of(' '));
    value.erase(value.find_last_not_of(' ') + 1);

    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    (*headers)[key] = value;

    return totalSize;
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

    if (showDetails && selected >= 0 &&
        selected < static_cast<int>(requests.size())) {
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
      for (int i = 0; i < static_cast<int>(requests.size()); i++) {
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
    if (selected < static_cast<int>(requests.size()) - 1) {
      selected++;
    }
  }

  void toggleDetails() { showDetails = !showDetails; }

  std::shared_ptr<HttpRequest> getSelected() {
    if (selected >= 0 && selected < static_cast<int>(requests.size())) {
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

template <typename R>
concept ConvertibleToStringViewRange =
    std::ranges::range<R> &&
    std::convertible_to<std::ranges::range_reference_t<R>, std::string_view>;

// HTTP Request Parser with variable support
class HttpRequestParser {
public:
  static std::vector<std::shared_ptr<HttpRequest>>
  parseContents(ConvertibleToStringViewRange auto &&range) {
    std::vector<std::shared_ptr<HttpRequest>> requests;

    // Clear variables for a fresh parse
    variables.clear();

    std::shared_ptr<HttpRequest> currentRequest = nullptr;
    bool inHeaders = false;
    bool inBody = false;
    std::string name;
    std::string body;

    for (std::string_view line : range) {
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
          std::string_view key = line.substr(0, colonPos);
          std::string_view value = line.substr(colonPos + 1);

          std::string_view trimmedKey = trimWhitespace(key);
          std::string_view trimmedValue = trimWhitespace(value);
          // Substitute variables in header values
          std::string transformedValue = substituteVariables(trimmedValue);

          currentRequest->addHeader(std::string(trimmedKey), transformedValue);
        }
      }
      // Parse body
      else if (inBody && currentRequest) {
        if (!body.empty()) {
          body += "\n";
        }
        body += substituteVariables(line);
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

  static std::vector<std::shared_ptr<HttpRequest>>
  parseFile(const std::string_view filename) {
    auto reader = create_mmap_reader((std::string(filename)));

    if (!reader->is_open()) {
      std::println(stderr, "Error: Could not open file {}", filename);
      return std::vector<std::shared_ptr<HttpRequest>>{};
    }

    return parseContents(*reader);
  }

  static std::vector<std::shared_ptr<HttpRequest>>
  parseString(const std::string_view string_content) {
    return parseContents(
        string_content | std::views::split('\n') |
        std::views::transform([](auto r) { return std::string_view(r); }));
  }

private:
  static std::map<std::string, std::string> variables;

  static std::string_view trimWhitespace(const std::string_view string) {
    size_t start = string.find_first_not_of(" \t");
    if (start == std::string_view::npos) {
      return std::string_view(); // Empty string
    }

    size_t end = string.find_last_not_of(" \t");
    return string.substr(start, end - start + 1);
  }

  // Parse a variable declaration line
  static void parseVariable(const std::string_view line) {
    // Remove leading @
    std::string_view varLine = line.substr(1);

    // Find the equal sign
    size_t equalPos = varLine.find('=');
    if (equalPos == std::string::npos) {
      return; // Invalid variable declaration
    }

    // Extract variable name and value
    std::string_view varName = trimWhitespace(varLine.substr(0, equalPos));
    std::string_view varValue = trimWhitespace(varLine.substr(equalPos + 1));

    // Handle quoted strings
    if (varValue.front() == '"' && varValue.back() == '"') {
      varValue = varValue.substr(1, varValue.length() - 2);
    }

    // Store the variable
    variables[std::string(varName)] = std::string(varValue);
  }

  // Substitute variables in a string without using regex
  static std::string substituteVariables(const std::string_view input) {
    std::string result = std::string(input);
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
};

// Initialize the static member
std::map<std::string, std::string> HttpRequestParser::variables;

struct LoadRequestOptions {
  bool shouldEval = false;
  bool shouldFeedFromStdin = false;
  bool showHelp = false;
  std::string evalString;
  std::string requestFile;
};

// Main application
class HttpRequestApp {
public:
  HttpRequestApp() : adapter(std::make_unique<CurlAdapter>()) {}

  bool loadRequests(const LoadRequestOptions &options) {

    auto requests =
        options.shouldFeedFromStdin
            ? HttpRequestParser::parseString(collectStreamLines(std::cin))
        : options.shouldEval
            ? HttpRequestParser::parseString(options.evalString)
            : HttpRequestParser::parseFile(options.requestFile);

    if (requests.empty()) {
      std::println(stderr, "No valid requests found.");
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

    auto input = create_terminal_input();

    while (true) {
      menu.display();

      int key = input->getKey();

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

          if (const auto response = adapter->doRequest(*request);
              response.has_value()) {
            std::println("Headers:");

            for (const auto &header : response->headers) {
              std::println("  {}: {}", header.first, header.second);
            }

            std::println("Status: {}", response->statusCode);
            std::println("Body:");
            std::println("{}\n", response->body.value_or("NOTHING"));
          } else {
            std::println(stderr, "Transport error: {}",
                         response.error().message);
          }

          std::print("Press any key to continue...");
          input->getKey();
        }
      }
    }

    // Clear screen before exiting
    std::print("\033[2J\033[H");
  }

private:
  RequestMenu menu;
  std::unique_ptr<RequestAdapter> adapter;

  static std::string collectStreamLines(std::istream &in) {
    std::string ret;
    ret.reserve(64 * 1024); // reserve 64 KB to reduce early reallocations

    char buffer[4096];
    while (in.read(buffer, sizeof(buffer)))
      ret.append(buffer, sizeof(buffer));

    ret.append(buffer, in.gcount());
    return ret;
  }
};

void print_usage(const std::string_view programName) {
  std::println("Usage: {} [OPTIONS] <http_request_file>", programName);
  std::println("       {} --eval <string> [OPTIONS]", programName);
  std::println("       {} --stdin [OPTIONS]\n", programName);
  std::println("A simple console application to load and run HTTP requests.\n");
  std::println("Input Sources (one must be provided):");
  std::println(
      "  <http_request_file>  Path to the file containing the HTTP request.");
  std::println("  --eval <string>      Takes the provided string as the "
               "request to evaluate.");
  std::println(
      "  --stdin              Reads the HTTP request from standard input.\n");
  std::println("General Options:");
  std::println(
      "  -h, --help           Displays this help message and exits.\n");
  std::println("Examples:");
  std::println("  # Run a request from a file");
  std::println("  {} request.txt\n", programName);
  std::println("  # Evaluate a string directly");
  std::println("  {} -e \"GET /api/users\"\n", programName);
  std::println("  # Pipe a request from another command");
  std::println("  cat request.txt | {} --stdin\n", programName);
}

// using ParseOptionsResult = std::expected<LoadRequestOptions, std::string>;
using ParseOptionsResult = std::expected<LoadRequestOptions, error>;

ParseOptionsResult parseOptions(int argc, char *argv[]) {
  LoadRequestOptions options;
  std::vector<std::string_view> args(argv, argv + argc);

  for (auto it = args.begin() + 1; it != args.end(); ++it) {
    const std::string_view arg = *it;

    if (arg == "-h" || arg == "--help") {
      options.showHelp = true;
      return options;
    }

    if (arg == "--stdin") {
      options.shouldFeedFromStdin = true;
      continue;
    }

    if (arg == "-e" || arg == "--eval") {
      if (it + 1 == args.end()) {
        return std::unexpected(
            error{.code = agatetepe_error::parse_error,
                  .message = "Error: The " + std::string(arg) +
                             " option requires a string argument."});
      }
      options.shouldEval = true;
      options.evalString = *(++it);
      continue;
    }

    // If the argument doesn't start with '-', it's the positional request file
    if (!arg.starts_with('-')) {
      if (!options.requestFile.empty()) {
        return std::unexpected(
            error{.code = agatetepe_error::parse_error,
                  .message = "Error: Multiple request files specified. Only "
                             "one is allowed."});
      }
      options.requestFile = arg;
    }
  }

  return options;
}

// Main function
int main(int argc, char *argv[]) {
  auto parseResult = parseOptions(argc, argv);

  if (!parseResult) {
    std::println(stderr, "{}", parseResult.error().message);
    return 1;
  }

  LoadRequestOptions options = std::move(parseResult.value());

  if (options.showHelp) {
    print_usage(argv[0]);
    return 0;
  }

  short input_sources_count = 0;
  if (!options.requestFile.empty())
    input_sources_count++;
  if (options.shouldEval)
    input_sources_count++;
  if (options.shouldFeedFromStdin)
    input_sources_count++;

  if (input_sources_count == 0) {
    std::println(stderr, "Error: no request source provided.");
    print_usage(argv[0]);
    return 1;
  }

  if (input_sources_count > 1) {
    std::println(stderr, "Error: Multiple request sources provided. Please use "
                         "only of: <file>, --eval or --stdin.");
    print_usage(argv[0]);
    return 1;
  }

  HttpRequestApp app;
  if (!app.loadRequests(options)) {
    return 1;
  }

  app.run();

  return 0;
}

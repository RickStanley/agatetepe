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

enum class e_agatetepe_error { unknown, parse_error, curl_error };

struct AgatetepeError {
  e_agatetepe_error code = e_agatetepe_error::unknown;
  std::string message;
};

static constexpr size_t string_length(const char *str) {
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
    static constexpr auto paren_length = string_length("(");
    // remove $
    std::string_view var_name = input.substr(1, input.length());
    auto param_start_pos = var_name.find('(');
    bool has_params = param_start_pos != std::string_view::npos;
    auto param_end_pos = var_name.find(')');

    if (param_end_pos == std::string_view::npos && has_params)
      return "";
    std::string_view prefix =
        var_name.substr(0, has_params ? param_start_pos : var_name.size());

    std::string_view params =
        has_params ? var_name.substr(param_start_pos + paren_length,
                                     param_end_pos - paren_length)
                   : "";

    return _generate_variable(prefix, params);
  }

private:
  static std::string _generate_variable(const std::string_view variable_type,
                                        const std::string_view params) {
    // TODO(stanley): may use a map
    if (variable_type == "uuid" || variable_type == "random.uuid") {
      return _generate_uuid();
    } else if (variable_type == "timestamp") {
      return _generate_timestamp();
    } else if (variable_type == "isoTimestamp") {
      return _generate_iso_timestamp();
    } else if (variable_type == "randomInt" ||
               variable_type == "random.integer") {
      return _generate_random_int(params);
    } else if (variable_type == "random.float") {
      return _generate_random_float(params);
    } else if (variable_type == "random.alphabetic") {
      return _generate_random_alphabetic(params);
    } else if (variable_type == "random.alphanumeric") {
      return _generate_random_alphanumeric(params);
    } else if (variable_type == "random.hexadecimal") {
      return _generate_random_hexadecimal(params);
    } else if (variable_type == "random.email") {
      return _generate_random_email();
    }

    return ""; // Unknown variable type
  }

  static std::string _generate_uuid() {
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

  static std::string _generate_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
            .count();
    return std::to_string(timestamp);
  }

  static std::string _generate_iso_timestamp() {
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

  static std::string _generate_random_int(const std::string_view params) {
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

  static std::string _generate_random_float(const std::string_view params) {
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

  static std::string
  _generate_random_alphabetic(const std::string_view params) {
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

  static std::string
  _generate_random_alphanumeric(const std::string_view params) {
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

  static std::string
  _generate_random_hexadecimal(const std::string_view params) {
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

  static std::string _generate_random_email() {
    std::string username = _generate_random_alphabetic("8");
    std::string domain = _generate_random_alphabetic("6");
    std::string tld = _generate_random_alphabetic("3");

    return std::format("{}@{}.{}", username, domain, tld);
  }
};

// Forward declarations
class HttpRequest;
class RequestAdapter;
class CurlAdapter;

// Plain Old Data
struct HttpResponse {
  long status_code = 0;
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

  void add_header(const std::string &key, const std::string &value) {
    headers[key] = value;
  }

  void set_body(const std::string &body) { this->body = body; }
};

// Abstract adapter for request engines
class RequestAdapter {
public:
  virtual ~RequestAdapter() = default;
  virtual std::expected<HttpResponse, AgatetepeError>
  do_request(const HttpRequest &request) = 0;
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

  std::expected<HttpResponse, AgatetepeError>
  do_request(const HttpRequest &request) override {
    CURL *curl = curl_easy_init();
    if (!curl) {
      return std::unexpected(
          AgatetepeError{.code = e_agatetepe_error::curl_error,
                         .message = "Failed to initialise cURL easy handler."});
    }

    std::string response_body;
    std::map<std::string, std::string> response_headers;
    struct curl_slist *headers_list = nullptr;

    // Set the URL
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, _curl_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);

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
      std::string header_string = header.first + ": " + header.second;
      headers_list = curl_slist_append(headers_list, header_string.c_str());
    }

    if (headers_list) {
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers_list);
    }

    // Perform the request
    CURLcode res = curl_easy_perform(curl);

    // Check for transport errors (e.g., network failure, couldn't resolve host)
    if (res != CURLE_OK) {
      // Cleanup resources before returning error
      curl_easy_cleanup(curl);
      curl_slist_free_all(headers_list);
      return std::unexpected(AgatetepeError{
          .code = e_agatetepe_error::curl_error,
          .message = std::format("curl_easy_perform() failed: {}",
                                 curl_easy_strerror(res))});
    }

    // Get the HTTP status code. This is now part of a successful transport.
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    // Cleanup resources
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers_list);

    // Construct and return the successful response object.
    // The caller is now responsible for checking the status code.
    HttpResponse response;
    response.status_code = httpCode;
    response.body = response_body;
    response.headers = std::move(response_headers);

    return response;
  }

private:
  static size_t _curl_write_callback(void *contents, size_t size, size_t nmemb,
                                     std::string *userp) {
    size_t total_size = size * nmemb;
    if (userp) {
      userp->append((char *)contents, total_size);
    }
    return total_size;
  }

  static size_t _curl_header_callback(char *buffer, size_t size, size_t nitems,
                                      void *userdata) {
    auto *headers = static_cast<std::map<std::string, std::string> *>(userdata);
    size_t total_size = size * nitems;

    std::string header_line(buffer, total_size);

    header_line.erase(header_line.find_last_not_of("\r\n") + 1);

    // Ignore empty lines and the HTTP status line (e.g., "HTTP/1.1 200 OK")
    if (header_line.empty() || header_line.find(':') == std::string::npos) {
      return total_size;
    }

    size_t colon_position = header_line.find(':');
    std::string key = header_line.substr(0, colon_position);
    std::string value = header_line.substr(colon_position + 1);

    // Trim
    key.erase(0, key.find_first_not_of(' '));
    key.erase(key.find_last_not_of(' ') + 1);
    value.erase(0, value.find_first_not_of(' '));
    value.erase(value.find_last_not_of(' ') + 1);

    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    (*headers)[key] = value;

    return total_size;
  }
};

// Terminal menu for selecting requests
class RequestMenu {
public:
  void add_request(std::shared_ptr<HttpRequest> request) {
    _requests.push_back(request);
  }

  void jump_to(int index) { _selected = index; }

  void display() {
    std::print("\033[2J\033[H");

    std::println("HTTP Request Selector");
    std::println("=====================\n");

    if (_requests.empty()) {
      std::println("No requests available.");
      return;
    }

    if (_show_details && _selected >= 0 &&
        _selected < static_cast<int>(_requests.size())) {
      auto request = _requests[_selected];
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
      for (int i = 0; i < static_cast<int>(_requests.size()); i++) {
        if (i == _selected) {
          std::print("> ");
        } else {
          std::print("  ");
        }

        if (!_requests[i]->name.empty()) {
          std::println("# {}", _requests[i]->name);
          std::print("    ");
        }
        std::println("{} {}", _requests[i]->method, _requests[i]->url);
      }

      std::println("\nPress 'd' to toggle details, arrow keys to navigate, "
                   "Enter to select, q to quit.");
    }
  }

  void move_up() {
    if (_selected > 0) {
      _selected--;
    }
  }

  void move_down() {
    if (_selected < static_cast<int>(_requests.size()) - 1) {
      _selected++;
    }
  }

  void toggle_details() { _show_details = !_show_details; }

  std::shared_ptr<HttpRequest> get_selected() {
    if (_selected >= 0 && _selected < static_cast<int>(_requests.size())) {
      return _requests[_selected];
    }
    return nullptr;
  }

  void reset() {
    _selected = 0;
    _show_details = false;
  }

  size_t size() const { return _requests.size(); }

private:
  std::vector<std::shared_ptr<HttpRequest>> _requests;
  int _selected = 0;
  bool _show_details = false;
};

template <typename R>
concept ConvertibleToStringViewRange =
    std::ranges::range<R> &&
    std::convertible_to<std::ranges::range_reference_t<R>, std::string_view>;

// HTTP Request Parser with variable support
class HttpRequestParser {
public:
  static std::vector<std::shared_ptr<HttpRequest>>
  parse_contents(ConvertibleToStringViewRange auto &&range) {
    std::vector<std::shared_ptr<HttpRequest>> requests;

    // Clear variables for a fresh parse
    _variables.clear();

    std::shared_ptr<HttpRequest> current_request = nullptr;
    bool in_headers = false;
    bool in_body = false;
    std::string name;
    std::string body;

    for (std::string_view line : range) {
      if (line.starts_with("# @name")) {
        constexpr auto nameSize = string_length("# @name");
        name = std::string_view(line).substr(nameSize + 1);
        continue;
      }

      // Skip comments
      if (line.starts_with("#") || line.starts_with("//")) {
        continue;
      }

      // Parse variable declarations
      if (line.find("@") == 0) {
        _parse_variable(line);
        continue;
      }

      // Skip empty lines
      if (line.empty()) {
        if (current_request && in_headers) {
          in_headers = false;
          in_body = true;
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
        if (current_request) {
          if (in_body && !body.empty()) {
            current_request->set_body(body);
          }
          requests.push_back(current_request);
        }

        // Parse method and URL, substituting variables
        std::string processed_line = _substitue_variables(line);
        size_t space_pos = processed_line.find(' ');
        std::string method = processed_line.substr(0, space_pos);
        std::string url = processed_line.substr(space_pos + 1);

        // Create new request
        current_request = std::make_shared<HttpRequest>(method, url, name);
        in_headers = true;
        in_body = false;
        body = "";
        name = "";
      }
      // Parse headers
      else if (in_headers && current_request) {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
          std::string_view key = line.substr(0, colon_pos);
          std::string_view value = line.substr(colon_pos + 1);

          std::string_view trimmed_key = _trim_whitespace(key);
          std::string_view trimmed_value = _trim_whitespace(value);
          // Substitute variables in header values
          std::string transformed_value = _substitue_variables(trimmed_value);

          current_request->add_header(std::string(trimmed_key),
                                      transformed_value);
        }
      }
      // Parse body
      else if (in_body && current_request) {
        if (!body.empty()) {
          body += "\n";
        }
        body += _substitue_variables(line);
      }
    }

    // Add the last request if exists
    if (current_request) {
      if (in_body && !body.empty()) {
        // Substitute variables in body
        body = _substitue_variables(body);
        current_request->set_body(body);
      }
      requests.push_back(current_request);
    }

    return requests;
  }

  static std::vector<std::shared_ptr<HttpRequest>>
  parse_file(const std::string_view filename) {
    auto reader = create_mmap_reader((std::string(filename)));

    if (!reader->is_open()) {
      std::println(stderr, "Error: Could not open file {}", filename);
      return std::vector<std::shared_ptr<HttpRequest>>{};
    }

    return parse_contents(*reader);
  }

  static std::vector<std::shared_ptr<HttpRequest>>
  parse_string(const std::string_view string_content) {
    return parse_contents(
        string_content | std::views::split('\n') |
        std::views::transform([](auto r) { return std::string_view(r); }));
  }

private:
  static std::map<std::string, std::string> _variables;

  static std::string_view _trim_whitespace(const std::string_view string) {
    size_t start = string.find_first_not_of(" \t");
    if (start == std::string_view::npos) {
      return std::string_view(); // Empty string
    }

    size_t end = string.find_last_not_of(" \t");
    return string.substr(start, end - start + 1);
  }

  // Parse a variable declaration line
  static void _parse_variable(const std::string_view line) {
    // Remove leading @
    std::string_view var_line = line.substr(1);

    // Find the equal sign
    size_t equal_pos = var_line.find('=');
    if (equal_pos == std::string::npos) {
      return; // Invalid variable declaration
    }

    // Extract variable name and value
    std::string_view var_name = _trim_whitespace(var_line.substr(0, equal_pos));
    std::string_view var_value =
        _trim_whitespace(var_line.substr(equal_pos + 1));

    // Handle quoted strings
    if (var_value.front() == '"' && var_value.back() == '"') {
      var_value = var_value.substr(1, var_value.length() - 2);
    }

    // Store the variable
    _variables[std::string(var_name)] = std::string(var_value);
  }

  // Substitute variables in a string without using regex
  static std::string _substitue_variables(const std::string_view input) {
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
      std::string var_name = result.substr(start + 2, end - start - 2);

      // Find the replacement value
      std::string replacement =
          var_name.starts_with("$") ? DynamicVariableResolver::resolve(var_name)
          : _variables.count(var_name) ? _variables[var_name]
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
std::map<std::string, std::string> HttpRequestParser::_variables;

struct LoadRequestOptions {
  bool should_eval = false;
  bool should_feed_from_stdin = false;
  bool show_help = false;
  std::optional<short> pick_index;
  std::string eval_string;
  std::string request_file;
};

// Main application
class HttpRequestApp {
public:
  HttpRequestApp() : _adapter(std::make_unique<CurlAdapter>()) {}

  bool load_requests(const LoadRequestOptions &options) {

    auto requests =
        options.should_feed_from_stdin
            ? HttpRequestParser::parse_string(_collect_stream_lines(std::cin))
        : options.should_eval
            ? HttpRequestParser::parse_string(options.eval_string)
            : HttpRequestParser::parse_file(options.request_file);

    if (requests.empty()) {
      std::println(stderr, "No valid requests found.");
      return false;
    }

    for (const auto &request : requests) {
      _menu.add_request(request);
    }

    return true;
  }

  void run() {
    if (_menu.size() == 0) {
      std::println("No requests to display. Exiting.");
      return;
    }

    auto input = create_terminal_input();

    while (true) {
      _menu.display();

      int key = input->get_key();

      // Handle special keys
      if (key == 1) { // Up arrow
        _menu.move_up();
      } else if (key == 2) { // Down arrow
        _menu.move_down();
      } else if (key == 'q' || key == 'Q') {
        break;
      } else if (key == 'd' || key == 'D') {
        _menu.toggle_details();
      } else if (key == '\n') { // Enter key
        auto request = _menu.get_selected();
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

          if (const auto response = _adapter->do_request(*request);
              response.has_value()) {
            std::println("Headers:");

            for (const auto &header : response->headers) {
              std::println("  {}: {}", header.first, header.second);
            }

            std::println("Status: {}", response->status_code);
            std::println("Body:");
            std::println("{}\n", response->body.value_or("NOTHING"));
          } else {
            std::println(stderr, "Transport error: {}",
                         response.error().message);
          }

          std::print("Press any key to continue...");
          input->get_key();
        }
      }
    }

    // Clear screen before exiting
    std::print("\033[2J\033[H");
  }

  // TODO(stanley): maybe merge to a common/shared function with `run`
  int request_pick_at(ushort index) {
    if (index > _menu.size()) {
      std::println(stderr,
                   "Error: out of range of requests available, you requested "
                   "{} but there are {} requests.",
                   index, _menu.size());
      return 1;
    }

    _menu.jump_to(index - 1);
    auto request = _menu.get_selected();

    if (const auto response = _adapter->do_request(*request);
        response.has_value()) {
      std::println("Headers:");

      for (const auto &header : response->headers) {
        std::println("  {}: {}", header.first, header.second);
      }

      std::println("Status: {}", response->status_code);
      std::println("Body:");
      std::println("{}", response->body.value_or("NOTHING"));
    } else {
      std::println(stderr, "Transport error: {}", response.error().message);

      return 1;
    }

    return 0;
  }

private:
  RequestMenu _menu;
  std::unique_ptr<RequestAdapter> _adapter;

  static std::string _collect_stream_lines(std::istream &in) {
    std::string ret;
    ret.reserve(64 * 1024); // reserve 64 KB to reduce early reallocations

    char buffer[4096];
    while (in.read(buffer, sizeof(buffer)))
      ret.append(buffer, sizeof(buffer));

    ret.append(buffer, in.gcount());
    return ret;
  }
};

void print_usage(const std::string_view program_name) {
  std::println("Usage: {} [OPTIONS] <http_request_file>", program_name);
  std::println("       {} --eval <string> [OPTIONS]", program_name);
  std::println("       {} --stdin [OPTIONS]\n", program_name);
  std::println("A simple console application to load and run HTTP requests.\n");
  std::println("Input Sources (one must be provided):");
  std::println(
      "  <http_request_file>  Path to the file containing the HTTP request.");
  std::println("  --eval <string>      Takes the provided string as the "
               "request to evaluate.");
  std::println(
      "  --stdin              Reads the HTTP request from standard input.\n");
  std::println("General Options:");
  std::println("  -p, --pick-index     Picks a specific request at index if "
               "possible.\n");
  std::println(
      "  -h, --help           Displays this help message and exits.\n");
  std::println("Examples:");
  std::println("  # Run a request from a file");
  std::println("  {} request.txt\n", program_name);
  std::println("  # Evaluate a string directly");
  std::println("  {} -e \"GET /api/users\"\n", program_name);
  std::println("  # Pipe a request from another command");
  std::println("  cat request.txt | {} --stdin\n", program_name);
  std::println(
      "  # Picks the request at index 1 (first request, top-down wise)");
  std::println("  {} --pick-index 1 requests.http\n", program_name);
}

// using ParseOptionsResult = std::expected<LoadRequestOptions, std::string>;
using ParseOptionsResult = std::expected<LoadRequestOptions, AgatetepeError>;

ParseOptionsResult parse_options(int argc, char *argv[]) {
  LoadRequestOptions options;
  std::vector<std::string_view> args(argv, argv + argc);

  for (auto it = args.begin() + 1; it != args.end(); ++it) {
    const std::string_view arg = *it;

    if (arg == "-h" || arg == "--help") {
      options.show_help = true;
      return options;
    }

    if (arg == "--stdin") {
      options.should_feed_from_stdin = true;
      continue;
    }

    if (arg == "-p" || arg == "--pick-index") {
      if (it + 1 == args.end()) {
      pick_option_failure:
        return std::unexpected(AgatetepeError{
            .code = e_agatetepe_error::parse_error,
            .message =
                "Error: The " + std::string(arg) +
                " option requires a non-negative/non-zero number argument."});
      }

      std::stringstream convertor;
      short number = 0;

      convertor << *(++it);
      convertor >> number;

      if (convertor.fail() || number <= 0) {
        goto pick_option_failure;
      }

      options.pick_index = number;
      continue;
    }

    if (arg == "-e" || arg == "--eval") {
      if (it + 1 == args.end()) {
        return std::unexpected(
            AgatetepeError{.code = e_agatetepe_error::parse_error,
                           .message = "Error: The " + std::string(arg) +
                                      " option requires a string argument."});
      }
      options.should_eval = true;
      options.eval_string = *(++it);
      continue;
    }

    // If the argument doesn't start with '-', it's the positional request file
    if (!arg.starts_with('-')) {
      if (!options.request_file.empty()) {
        return std::unexpected(AgatetepeError{
            .code = e_agatetepe_error::parse_error,
            .message = "Error: Multiple request files specified. Only "
                       "one is allowed."});
      }
      options.request_file = arg;
    }
  }

  return options;
}

// Main function
int main(int argc, char *argv[]) {
  auto parse_result = parse_options(argc, argv);

  if (!parse_result) {
    std::println(stderr, "{}", parse_result.error().message);
    return 1;
  }

  LoadRequestOptions options = std::move(parse_result.value());

  if (options.show_help) {
    print_usage(argv[0]);
    return 0;
  }

  short input_sources_count = 0;
  if (!options.request_file.empty())
    input_sources_count++;
  if (options.should_eval)
    input_sources_count++;
  if (options.should_feed_from_stdin)
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
  if (!app.load_requests(options)) {
    return 1;
  }

  if (options.pick_index.has_value()) {
    return app.request_pick_at(options.pick_index.value());
  } else {
    app.run();
  }

  return 0;
}

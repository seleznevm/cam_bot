// main.cpp

#include <iostream>
#include <string>
#include <mosquitto.h>
#include <curl/curl.h>
#include <fstream>
#include <chrono>
#include <ctime>

// Параметры MQTT
const char* MQTT_HOST = "test.mosquitto.org";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC = "apartment/security";
const char* MQTT_CLIENT_ID = "WindowsClient";

// Параметры ESP32S
const std::string PHOTO_URL = "http://esp32.local/latest.jpg";
const std::string PHOTO_FILENAME = "latest.jpg";

// Параметры Telegram
const std::string BOT_TOKEN = "7265233742:AAE5K6QChXq-90YjF0C-wGXG8XpLc7Hlc_4";
const std::string CHAT_ID = "@Security camera"; // Замените на ваш chat_id или имя канала

// Функции для работы с curl
size_t write_data(void* ptr, size_t size, size_t nmemb, void* stream) {
    std::ofstream* out = static_cast<std::ofstream*>(stream);
    size_t totalSize = size * nmemb;
    out->write(static_cast<char*>(ptr), totalSize);
    return totalSize;
}

// Функция для загрузки фотографии с ESP32S
void download_latest_photo(const std::string& url, const std::string& filename) {
    CURL* curl = curl_easy_init();
    if (curl) {
        std::ofstream outfile(filename, std::ios::binary);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outfile);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
            std::cerr << "Ошибка загрузки фотографии: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        outfile.close();
    }
}

// Функция для получения текущей даты и времени
std::string get_current_datetime() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    // Удаляем символ новой строки из ctime
    std::string datetime = std::ctime(&now_c);
    if (!datetime.empty() && datetime[datetime.length() - 1] == '\n') {
        datetime.erase(datetime.length() - 1);
    }
    return datetime;
}

// Функция для отправки фотографии в Telegram
void send_photo_to_telegram(const std::string& filename, const std::string& caption) {
    CURL* curl = curl_easy_init();
    if (curl) {
        struct curl_mime* form = NULL;
        struct curl_mimepart* field = NULL;

        std::string url = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendPhoto";

        form = curl_mime_init(curl);

        // Добавляем chat_id
        field = curl_mime_addpart(form);
        curl_mime_name(field, "chat_id");
        curl_mime_data(field, CHAT_ID.c_str(), CURL_ZERO_TERMINATED);

        // Добавляем подпись (caption)
        field = curl_mime_addpart(form);
        curl_mime_name(field, "caption");
        curl_mime_data(field, caption.c_str(), CURL_ZERO_TERMINATED);

        // Добавляем фотографию
        field = curl_mime_addpart(form);
        curl_mime_name(field, "photo");
        curl_mime_filedata(field, filename.c_str());

        // Настраиваем запрос
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);

        // Выполняем запрос
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
            std::cerr << "Ошибка отправки фотографии в Telegram: " << curl_easy_strerror(res) << std::endl;

        // Освобождаем ресурсы
        curl_easy_cleanup(curl);
        curl_mime_free(form);
    }
}

// Функция обработки нового сообщения MQTT
void on_message(struct mosquitto* mosq, void* userdata, const struct mosquitto_message* message) {
    std::string payload((char*)message->payload, message->payloadlen);
    std::cout << "Получено сообщение: " << payload << std::endl;

    if (payload == "human_detected") {
        std::cout << "Обнаружен человек. Начинается обработка..." << std::endl;

        // Загрузка фотографии с ESP32S
        download_latest_photo(PHOTO_URL, PHOTO_FILENAME);

        // Получение текущей даты и времени
        std::string datetime = get_current_datetime();

        // Отправка фотографии в Telegram
        send_photo_to_telegram(PHOTO_FILENAME, "Дата и время: " + datetime);
    }
}

// Функция обработки события подключения к MQTT брокеру
void on_connect(struct mosquitto* mosq, void* userdata, int rc) {
    if (rc == 0) {
        std::cout << "Успешно подключено к MQTT брокеру." << std::endl;
        mosquitto_subscribe(mosq, NULL, MQTT_TOPIC, 1);
    }
    else {
        std::cerr << "Ошибка подключения к MQTT брокеру: " << mosquitto_strerror(rc) << std::endl;
    }
}

int main() {
    // Инициализация библиотеки Mosquitto
    mosquitto_lib_init();

    // Создание клиента Mosquitto
    struct mosquitto* mosq = mosquitto_new(MQTT_CLIENT_ID, true, NULL);
    if (!mosq) {
        std::cerr << "Не удалось создать MQTT клиент." << std::endl;
        return 1;
    }

    // Установка callback-функций
    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    // Подключение к MQTT брокеру
    int rc = mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "Не удалось подключиться к MQTT брокеру: " << mosquitto_strerror(rc) << std::endl;
        mosquitto_destroy(mosq);
        return 1;
    }

    // Запуск цикла обработки сообщений
    mosquitto_loop_start(mosq);

    std::cout << "Нажмите Enter для выхода..." << std::endl;
    std::cin.get();

    // Остановка цикла и освобождение ресурсов
    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}

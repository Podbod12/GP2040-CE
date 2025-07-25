#include "config.pb.h"
#include "base64.h"

#include "drivermanager.h"
#include "storagemanager.h"
#include "eventmanager.h"
#include "layoutmanager.h"
#include "peripheralmanager.h"
#include "system.h"
#include "config_utils.h"
#include "types.h"
#include "version.h"

#include "neopicoleds.h"

#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <set>

#include <pico/types.h>

// HTTPD Includes
#include <ArduinoJson.h>
#include "rndis.h"
#include "fs.h"
#include "fscustom.h"
#include "fsdata.h"
#include "lwip/apps/httpd.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "addons/input_macro.h"

#define PATH_CGI_ACTION "/cgi/action"

#define LWIP_HTTPD_POST_MAX_PAYLOAD_LEN (1024 * 16)

extern struct fsdata_file file__index_html[];

const static char* spaPaths[] = { "/animation", "/backup", "/display-config", "/leds", "/pin-mapping", "/settings", "/reset-settings", "/add-ons", "/macro", "/peripheral-mapping" };
const static char* excludePaths[] = { "/css", "/images", "/js", "/static" };
const static uint32_t rebootDelayMs = 500;
static string http_post_uri;
static char http_post_payload[LWIP_HTTPD_POST_MAX_PAYLOAD_LEN];
static uint16_t http_post_payload_len = 0;

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K>
static void __attribute__((noinline)) readDoc(T& var, const DynamicJsonDocument& doc, const K& key)
{
    var = doc[key];
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K0, typename K1>
static void __attribute__((noinline)) readDoc(T& var, const DynamicJsonDocument& doc, const K0& key0, const K1& key1)
{
    var = doc[key0][key1];
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K0, typename K1, typename K2>
static void __attribute__((noinline)) readDoc(T& var, const DynamicJsonDocument& doc, const K0& key0, const K1& key1, const K2& key2)
{
    var = doc[key0][key1][key2];
}

// Don't inline this function, we do not want to consume stack space in the calling function
static bool __attribute__((noinline)) hasValue(const DynamicJsonDocument& doc, const char* key0, const char* key1)
{
    return doc[key0][key1] != nullptr;
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T>
static void __attribute__((noinline)) docToValue(T& value, const DynamicJsonDocument& doc, const char* key)
{
    if (doc[key] != nullptr)
    {
        value = doc[key];
    }
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T>
static void __attribute__((noinline)) docToValue(T& value, const DynamicJsonDocument& doc, const char* key0, const char* key1)
{
    if (doc[key0][key1] != nullptr)
    {
        value = doc[key0][key1];
    }
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T>
static void __attribute__((noinline)) docToValue(T& value, const DynamicJsonDocument& doc, const char* key0, const char* key1, const char* key2)
{
    if (doc[key0][key1][key2] != nullptr)
    {
        value = doc[key0][key1][key2];
    }
}

// Don't inline this function, we do not want to consume stack space in the calling function
static void __attribute__((noinline)) cleanAddonGpioMappings(Pin_t& addonPin, Pin_t oldAddonPin)
{
    GpioMappingInfo* gpioMappings = Storage::getInstance().getGpioMappings().pins;
    ProfileOptions& profiles = Storage::getInstance().getProfileOptions();

    // if the new addon pin value is valid, mark it assigned in GpioMappings
    if (isValidPin(addonPin))
    {
        gpioMappings[addonPin].action = GpioAction::ASSIGNED_TO_ADDON;
        profiles.gpioMappingsSets[0].pins[addonPin].action = GpioAction::ASSIGNED_TO_ADDON;
        profiles.gpioMappingsSets[1].pins[addonPin].action = GpioAction::ASSIGNED_TO_ADDON;
        profiles.gpioMappingsSets[2].pins[addonPin].action = GpioAction::ASSIGNED_TO_ADDON;
    } else {
        // -1 is our de facto value for "not assigned" in addons
        addonPin = -1;
    }

    // either way now, the addon's pin config is set to its real value, if the
    // old value is a real pin (and different), we should unset it
    if (isValidPin(oldAddonPin) && oldAddonPin != addonPin)
    {
        gpioMappings[oldAddonPin].action = GpioAction::NONE;
        profiles.gpioMappingsSets[0].pins[oldAddonPin].action = GpioAction::NONE;
        profiles.gpioMappingsSets[1].pins[oldAddonPin].action = GpioAction::NONE;
        profiles.gpioMappingsSets[2].pins[oldAddonPin].action = GpioAction::NONE;
    }
}

// Don't inline this function, we do not want to consume stack space in the calling function
static void __attribute__((noinline)) docToPin(Pin_t& pin, const DynamicJsonDocument& doc, const char* key)
{
    Pin_t oldPin = pin;
    if (doc.containsKey(key))
    {
        pin = doc[key];
        cleanAddonGpioMappings(pin, oldPin);
    }
}

// Don't inline this function, we do not want to consume stack space in the calling function
static void __attribute__((noinline)) docToPin(Pin_t& pin, const DynamicJsonDocument& doc, const char* key0, const char* key1)
{
    Pin_t oldPin = pin;
    if (doc.containsKey(key0) && doc[key0].containsKey(key1))
    {
        pin = doc[key0][key1];
        cleanAddonGpioMappings(pin, oldPin);
    }
}

// Don't inline this function, we do not want to consume stack space in the calling function
static void __attribute__((noinline)) docToPin(Pin_t& pin, const DynamicJsonDocument& doc, const char* key0, const char* key1, const char* key2)
{
    Pin_t oldPin = pin;
    if (doc.containsKey(key0) && doc[key0].containsKey(key1) && doc[key0][key1].containsKey(key2))
    {
        pin = doc[key0][key1][key2];
        cleanAddonGpioMappings(pin, oldPin);
    }
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K>
static void __attribute__((noinline)) writeDoc(DynamicJsonDocument& doc, const K& key, const T& var)
{
    doc[key] = var;
}

// Don't inline this function, we do not want to consume stack space in the calling function
// Web-config frontend compatibility workaround
template <typename K>
static void __attribute__((noinline)) writeDoc(DynamicJsonDocument& doc, const K& key, const bool& var)
{
    doc[key] = var ? 1 : 0;
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K0, typename K1>
static void __attribute__((noinline)) writeDoc(DynamicJsonDocument& doc, const K0& key0, const K1& key1, const T& var)
{
    doc[key0][key1] = var;
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K0, typename K1, typename K2>
static void __attribute__((noinline)) writeDoc(DynamicJsonDocument& doc, const K0& key0, const K1& key1, const K2& key2, const T& var)
{
    doc[key0][key1][key2] = var;
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K0, typename K1, typename K2, typename K3>
static void __attribute__((noinline)) writeDoc(DynamicJsonDocument& doc, const K0& key0, const K1& key1, const K2& key2, const K3& key3, const T& var)
{
    doc[key0][key1][key2][key3] = var;
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K0, typename K1, typename K2, typename K3, typename K4>
static void __attribute__((noinline)) writeDoc(DynamicJsonDocument& doc, const K0& key0, const K1& key1, const K2& key2, const K3& key3, const K4& key4, const T& var)
{
    doc[key0][key1][key2][key3][key4] = var;
}

static int32_t cleanPin(int32_t pin) { return isValidPin(pin) ? pin : -1; }

enum class HttpStatusCode
{
    _200,
    _400,
    _500,
};

struct DataAndStatusCode
{
    DataAndStatusCode(string&& data, HttpStatusCode statusCode) :
        data(std::move(data)),
        statusCode(statusCode)
    {}

    string data;
    HttpStatusCode statusCode;
};

// **** WEB SERVER Overrides and Special Functionality ****
int set_file_data(fs_file* file, const DataAndStatusCode& dataAndStatusCode)
{
    static string returnData;

    const char* statusCodeStr = "";
    switch (dataAndStatusCode.statusCode)
    {
        case HttpStatusCode::_200: statusCodeStr = "200 OK"; break;
        case HttpStatusCode::_400: statusCodeStr = "400 Bad Request"; break;
        case HttpStatusCode::_500: statusCodeStr = "500 Internal Server Error"; break;
    }

    returnData.clear();
    returnData.append("HTTP/1.0 ");
    returnData.append(statusCodeStr);
    returnData.append("\r\n");
    returnData.append(
        "Server: GP2040-CE " GP2040VERSION "\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: "
    );
    returnData.append(std::to_string(dataAndStatusCode.data.length()));
    returnData.append("\r\n\r\n");
    returnData.append(dataAndStatusCode.data);

    file->data = returnData.c_str();
    file->len = returnData.size();
    file->index = file->len;
    file->http_header_included = file->http_header_included;
    file->pextension = NULL;

    return 1;
}

int set_file_data(fs_file *file, string&& data)
{
    if (data.empty())
        return 0;
    return set_file_data(file, DataAndStatusCode(std::move(data), HttpStatusCode::_200));
}

DynamicJsonDocument get_post_data()
{
    DynamicJsonDocument doc(LWIP_HTTPD_POST_MAX_PAYLOAD_LEN);
    deserializeJson(doc, http_post_payload, http_post_payload_len);
    return doc;
}

void save_hotkey(HotkeyEntry* hotkey, const DynamicJsonDocument& doc, const string hotkey_key)
{
    readDoc(hotkey->auxMask, doc, hotkey_key, "auxMask");
    uint32_t buttonsMask = doc[hotkey_key]["buttonsMask"];
    uint32_t dpadMask = 0;
    if (buttonsMask & GAMEPAD_MASK_DU) {
        dpadMask |= GAMEPAD_MASK_UP;
    }
    if (buttonsMask & GAMEPAD_MASK_DD) {
        dpadMask |= GAMEPAD_MASK_DOWN;
    }
    if (buttonsMask & GAMEPAD_MASK_DL) {
        dpadMask |= GAMEPAD_MASK_LEFT;
    }
    if (buttonsMask & GAMEPAD_MASK_DR) {
        dpadMask |= GAMEPAD_MASK_RIGHT;
    }
    buttonsMask &= ~(GAMEPAD_MASK_DU | GAMEPAD_MASK_DD | GAMEPAD_MASK_DL | GAMEPAD_MASK_DR);
    hotkey->dpadMask = dpadMask;
    hotkey->buttonsMask = buttonsMask;
    readDoc(hotkey->action, doc, hotkey_key, "action");
}

void load_hotkey(const HotkeyEntry* hotkey, DynamicJsonDocument& doc, const string hotkey_key)
{
    writeDoc(doc, hotkey_key, "auxMask", hotkey->auxMask);
    uint32_t buttonsMask = hotkey->buttonsMask;
    if (hotkey->dpadMask & GAMEPAD_MASK_UP) {
        buttonsMask |= GAMEPAD_MASK_DU;
    }
    if (hotkey->dpadMask & GAMEPAD_MASK_DOWN) {
        buttonsMask |= GAMEPAD_MASK_DD;
    }
    if (hotkey->dpadMask & GAMEPAD_MASK_LEFT) {
        buttonsMask |= GAMEPAD_MASK_DL;
    }
    if (hotkey->dpadMask & GAMEPAD_MASK_RIGHT) {
        buttonsMask |= GAMEPAD_MASK_DR;
    }
    writeDoc(doc, hotkey_key, "buttonsMask", buttonsMask);
    writeDoc(doc, hotkey_key, "action", hotkey->action);
}

// LWIP callback on HTTP POST to validate the URI
err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                       uint16_t http_request_len, int content_len, char *response_uri,
                       uint16_t response_uri_len, uint8_t *post_auto_wnd)
{
    LWIP_UNUSED_ARG(http_request);
    LWIP_UNUSED_ARG(http_request_len);
    LWIP_UNUSED_ARG(content_len);
    LWIP_UNUSED_ARG(response_uri);
    LWIP_UNUSED_ARG(response_uri_len);
    LWIP_UNUSED_ARG(post_auto_wnd);

    if (!uri || strncmp(uri, "/api", 4) != 0) {
        return ERR_ARG;
    }

    http_post_uri = uri;
    http_post_payload_len = 0;
    memset(http_post_payload, 0, LWIP_HTTPD_POST_MAX_PAYLOAD_LEN);
    return ERR_OK;
}

// LWIP callback on HTTP POST to for receiving payload
err_t httpd_post_receive_data(void *connection, struct pbuf *p)
{
    LWIP_UNUSED_ARG(connection);

    // Cache the received data to http_post_payload
    while (p != NULL)
    {
        if (http_post_payload_len + p->len <= LWIP_HTTPD_POST_MAX_PAYLOAD_LEN)
        {
            MEMCPY(http_post_payload + http_post_payload_len, p->payload, p->len);
            http_post_payload_len += p->len;
        }
        else // Buffer overflow
        {
            http_post_payload_len = 0xffff;
            break;
        }

        p = p->next;
    }

    // Need to release memory here or will leak
    pbuf_free(p);

    // If the buffer overflows, error out
    if (http_post_payload_len == 0xffff) {
        return ERR_BUF;
    }

    return ERR_OK;
}

// LWIP callback to set the HTTP POST response_uri, which can then be looked up via the fs_custom callbacks
void httpd_post_finished(void *connection, char *response_uri, uint16_t response_uri_len)
{
    LWIP_UNUSED_ARG(connection);

    if (http_post_payload_len != 0xffff) {
        strncpy(response_uri, http_post_uri.c_str(), response_uri_len);
        response_uri[response_uri_len - 1] = '\0';
    }
}

void addUsedPinsArray(DynamicJsonDocument& doc)
{
    auto usedPins = doc.createNestedArray("usedPins");

    GpioMappingInfo* gpioMappings = Storage::getInstance().getGpioMappings().pins;
    for (unsigned int pin = 0; pin < NUM_BANK0_GPIOS; pin++) {
        // NOTE: addons in webconfig break by seeing their own pins here; if/when they
        // are refactored to ignore their own pins from this list, we can include them
        if (gpioMappings[pin].action != GpioAction::NONE &&
                gpioMappings[pin].action != GpioAction::ASSIGNED_TO_ADDON) {
            usedPins.add(pin);
        }
    }
}

std::string serialize_json(DynamicJsonDocument &doc)
{
    std::string data;
    serializeJson(doc, data);
    return data;
}

std::string getUsedPins()
{
    const size_t capacity = JSON_OBJECT_SIZE(100);
    DynamicJsonDocument doc(capacity);
    addUsedPinsArray(doc);
    return serialize_json(doc);
}

std::string setDisplayOptions(DisplayOptions& displayOptions)
{
    DynamicJsonDocument doc = get_post_data();
    readDoc(displayOptions.enabled, doc, "enabled");
    readDoc(displayOptions.flip, doc, "flipDisplay");
    readDoc(displayOptions.invert, doc, "invertDisplay");
    readDoc(displayOptions.buttonLayout, doc, "buttonLayout");
    readDoc(displayOptions.buttonLayoutRight, doc, "buttonLayoutRight");
    readDoc(displayOptions.splashMode, doc, "splashMode");
    readDoc(displayOptions.splashChoice, doc, "splashChoice");
    readDoc(displayOptions.splashDuration, doc, "splashDuration");
    readDoc(displayOptions.displaySaverTimeout, doc, "displaySaverTimeout");
    readDoc(displayOptions.displaySaverMode, doc, "displaySaverMode");
    readDoc(displayOptions.buttonLayoutOrientation, doc, "buttonLayoutOrientation");
    readDoc(displayOptions.turnOffWhenSuspended, doc, "turnOffWhenSuspended");
    readDoc(displayOptions.inputMode, doc, "inputMode");
    readDoc(displayOptions.turboMode, doc, "turboMode");
    readDoc(displayOptions.dpadMode, doc, "dpadMode");
    readDoc(displayOptions.socdMode, doc, "socdMode");
    readDoc(displayOptions.macroMode, doc, "macroMode");
    readDoc(displayOptions.profileMode, doc, "profileMode");
    readDoc(displayOptions.inputHistoryEnabled, doc, "inputHistoryEnabled");
    readDoc(displayOptions.inputHistoryLength, doc, "inputHistoryLength");
    readDoc(displayOptions.inputHistoryCol, doc, "inputHistoryCol");
    readDoc(displayOptions.inputHistoryRow, doc, "inputHistoryRow");

    readDoc(displayOptions.buttonLayoutCustomOptions.paramsLeft.layout, doc, "buttonLayoutCustomOptions", "params", "layout");
    readDoc(displayOptions.buttonLayoutCustomOptions.paramsLeft.common.startX, doc, "buttonLayoutCustomOptions", "params", "startX");
    readDoc(displayOptions.buttonLayoutCustomOptions.paramsLeft.common.startY, doc, "buttonLayoutCustomOptions", "params", "startY");
    readDoc(displayOptions.buttonLayoutCustomOptions.paramsLeft.common.buttonRadius, doc, "buttonLayoutCustomOptions", "params", "buttonRadius");
    readDoc(displayOptions.buttonLayoutCustomOptions.paramsLeft.common.buttonPadding, doc, "buttonLayoutCustomOptions", "params", "buttonPadding");

    readDoc(displayOptions.buttonLayoutCustomOptions.paramsRight.layout, doc, "buttonLayoutCustomOptions", "paramsRight", "layout");
    readDoc(displayOptions.buttonLayoutCustomOptions.paramsRight.common.startX, doc, "buttonLayoutCustomOptions", "paramsRight", "startX");
    readDoc(displayOptions.buttonLayoutCustomOptions.paramsRight.common.startY, doc, "buttonLayoutCustomOptions", "paramsRight", "startY");
    readDoc(displayOptions.buttonLayoutCustomOptions.paramsRight.common.buttonRadius, doc, "buttonLayoutCustomOptions", "paramsRight", "buttonRadius");
    readDoc(displayOptions.buttonLayoutCustomOptions.paramsRight.common.buttonPadding, doc, "buttonLayoutCustomOptions", "paramsRight", "buttonPadding");

    return serialize_json(doc);
}

std::string setDisplayOptions()
{
    std::string response = setDisplayOptions(Storage::getInstance().getDisplayOptions());
    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));
    return response;
}

std::string setPreviewDisplayOptions()
{
    std::string response = setDisplayOptions(Storage::getInstance().getDisplayOptions());
    return response;
}

std::string getDisplayOptions() // Manually set Document Attributes for the display
{
    const size_t capacity = JSON_OBJECT_SIZE(100);
    DynamicJsonDocument doc(capacity);
    const DisplayOptions& displayOptions = Storage::getInstance().getDisplayOptions();
    writeDoc(doc, "enabled", displayOptions.enabled ? 1 : 0);
    writeDoc(doc, "flipDisplay", displayOptions.flip);
    writeDoc(doc, "invertDisplay", displayOptions.invert ? 1 : 0);
    writeDoc(doc, "buttonLayout", displayOptions.buttonLayout);
    writeDoc(doc, "buttonLayoutRight", displayOptions.buttonLayoutRight);
    writeDoc(doc, "splashMode", displayOptions.splashMode);
    writeDoc(doc, "splashChoice", displayOptions.splashChoice);
    writeDoc(doc, "splashDuration", displayOptions.splashDuration);
    writeDoc(doc, "displaySaverTimeout", displayOptions.displaySaverTimeout);
    writeDoc(doc, "displaySaverMode", displayOptions.displaySaverMode);
    writeDoc(doc, "buttonLayoutOrientation", displayOptions.buttonLayoutOrientation);
    writeDoc(doc, "turnOffWhenSuspended", displayOptions.turnOffWhenSuspended);
    writeDoc(doc, "inputMode", displayOptions.inputMode);
    writeDoc(doc, "turboMode", displayOptions.turboMode);
    writeDoc(doc, "dpadMode", displayOptions.dpadMode);
    writeDoc(doc, "socdMode", displayOptions.socdMode);
    writeDoc(doc, "macroMode", displayOptions.macroMode);
    writeDoc(doc, "profileMode", displayOptions.profileMode);
    writeDoc(doc, "inputHistoryEnabled", displayOptions.inputHistoryEnabled);
    writeDoc(doc, "inputHistoryLength", displayOptions.inputHistoryLength);
    writeDoc(doc, "inputHistoryCol", displayOptions.inputHistoryCol);
    writeDoc(doc, "inputHistoryRow", displayOptions.inputHistoryRow);

    writeDoc(doc, "buttonLayoutCustomOptions", "params", "layout", displayOptions.buttonLayoutCustomOptions.paramsLeft.layout);
    writeDoc(doc, "buttonLayoutCustomOptions", "params", "startX", displayOptions.buttonLayoutCustomOptions.paramsLeft.common.startX);
    writeDoc(doc, "buttonLayoutCustomOptions", "params", "startY", displayOptions.buttonLayoutCustomOptions.paramsLeft.common.startY);
    writeDoc(doc, "buttonLayoutCustomOptions", "params", "buttonRadius", displayOptions.buttonLayoutCustomOptions.paramsLeft.common.buttonRadius);
    writeDoc(doc, "buttonLayoutCustomOptions", "params", "buttonPadding", displayOptions.buttonLayoutCustomOptions.paramsLeft.common.buttonPadding);

    writeDoc(doc, "buttonLayoutCustomOptions", "paramsRight", "layout", displayOptions.buttonLayoutCustomOptions.paramsRight.layout);
    writeDoc(doc, "buttonLayoutCustomOptions", "paramsRight", "startX", displayOptions.buttonLayoutCustomOptions.paramsRight.common.startX);
    writeDoc(doc, "buttonLayoutCustomOptions", "paramsRight", "startY", displayOptions.buttonLayoutCustomOptions.paramsRight.common.startY);
    writeDoc(doc, "buttonLayoutCustomOptions", "paramsRight", "buttonRadius", displayOptions.buttonLayoutCustomOptions.paramsRight.common.buttonRadius);
    writeDoc(doc, "buttonLayoutCustomOptions", "paramsRight", "buttonPadding", displayOptions.buttonLayoutCustomOptions.paramsRight.common.buttonPadding);

    return serialize_json(doc);
}

std::string getSplashImage()
{
    const DisplayOptions& displayOptions = Storage::getInstance().getDisplayOptions();
    const size_t capacity = JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(displayOptions.splashImage.size);
    DynamicJsonDocument doc(capacity);
    JsonArray splashImageArray = doc.createNestedArray("splashImage");
    copyArray(displayOptions.splashImage.bytes, displayOptions.splashImage.size, splashImageArray);
    return serialize_json(doc);
}

std::string setSplashImage()
{
    DynamicJsonDocument doc = get_post_data();

    DisplayOptions& displayOptions = Storage::getInstance().getDisplayOptions();

    std::string decoded;
    std::string base64String = doc["splashImage"];
    Base64::Decode(base64String, decoded);
    const size_t length = std::min(decoded.length(), sizeof(displayOptions.splashImage.bytes));

    memcpy(displayOptions.splashImage.bytes, decoded.data(), length);
    displayOptions.splashImage.size = length;

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return serialize_json(doc);
}

std::string setProfileOptions()
{
    DynamicJsonDocument doc = get_post_data();

    ProfileOptions& profileOptions = Storage::getInstance().getProfileOptions();
    GpioMappings& coreMappings = Storage::getInstance().getGpioMappings();
    JsonObject options = doc.as<JsonObject>();
    JsonArray alts = options["alternativePinMappings"];
    int altsIndex = 0;
    char pinName[6];
    for (JsonObject alt : alts) {
        for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++) {
            snprintf(pinName, 6, "pin%0*d", 2, pin);
            // setting a pin shouldn't change a new existing addon/reserved pin
            // but if the profile definition is new, we should still capture the addon/reserved state
            if (profileOptions.gpioMappingsSets[altsIndex].pins[pin].action != GpioAction::ASSIGNED_TO_ADDON &&
                    profileOptions.gpioMappingsSets[altsIndex].pins[pin].action != GpioAction::RESERVED &&
                    (GpioAction)alt[pinName]["action"] != GpioAction::RESERVED &&
                    (GpioAction)alt[pinName]["action"] != GpioAction::ASSIGNED_TO_ADDON) {
                profileOptions.gpioMappingsSets[altsIndex].pins[pin].action = (GpioAction)alt[pinName]["action"];
                profileOptions.gpioMappingsSets[altsIndex].pins[pin].customButtonMask = (uint32_t)alt[pinName]["customButtonMask"];
                profileOptions.gpioMappingsSets[altsIndex].pins[pin].customDpadMask = (uint32_t)alt[pinName]["customDpadMask"];
            } else if ((coreMappings.pins[pin].action == GpioAction::RESERVED &&
                        (GpioAction)alt[pinName]["action"] == GpioAction::RESERVED) ||
                    (coreMappings.pins[pin].action == GpioAction::ASSIGNED_TO_ADDON &&
                        (GpioAction)alt[pinName]["action"] == GpioAction::ASSIGNED_TO_ADDON)) {
                profileOptions.gpioMappingsSets[altsIndex].pins[pin].action = (GpioAction)alt[pinName]["action"];
            }
        }
        profileOptions.gpioMappingsSets[altsIndex].pins_count = NUM_BANK0_GPIOS;

        size_t profileLabelSize = sizeof(profileOptions.gpioMappingsSets[altsIndex].profileLabel);
        strncpy(profileOptions.gpioMappingsSets[altsIndex].profileLabel, alt["profileLabel"], profileLabelSize - 1);
        profileOptions.gpioMappingsSets[altsIndex].profileLabel[profileLabelSize - 1] = '\0';
        profileOptions.gpioMappingsSets[altsIndex].enabled = alt["enabled"];

        profileOptions.gpioMappingsSets_count = ++altsIndex;
        if (altsIndex > 2) break;
    }

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));
    return serialize_json(doc);
}

std::string getProfileOptions()
{
    const size_t capacity = JSON_OBJECT_SIZE(500);
    DynamicJsonDocument doc(capacity);

    const auto writePinDoc = [&](const int item, const char* key, const GpioMappingInfo& value) -> void
    {
        writeDoc(doc, "alternativePinMappings", item, key, "action", value.action);
        writeDoc(doc, "alternativePinMappings", item, key, "customButtonMask", value.customButtonMask);
        writeDoc(doc, "alternativePinMappings", item, key, "customDpadMask", value.customDpadMask);
    };

    ProfileOptions& profileOptions = Storage::getInstance().getProfileOptions();

    // return an empty list if no profiles are currently set, since we no longer populate by default
    if (profileOptions.gpioMappingsSets_count == 0) {
        doc.createNestedArray("alternativePinMappings");
    }

    for (int i = 0; i < profileOptions.gpioMappingsSets_count; i++) {
        // this looks duplicative, but something in arduinojson treats the doc
        // field string by reference so you can't be "clever" and do an snprintf
        // thing or else you only send the last field in the JSON
        writePinDoc(i, "pin00", profileOptions.gpioMappingsSets[i].pins[0]);
        writePinDoc(i, "pin01", profileOptions.gpioMappingsSets[i].pins[1]);
        writePinDoc(i, "pin02", profileOptions.gpioMappingsSets[i].pins[2]);
        writePinDoc(i, "pin03", profileOptions.gpioMappingsSets[i].pins[3]);
        writePinDoc(i, "pin04", profileOptions.gpioMappingsSets[i].pins[4]);
        writePinDoc(i, "pin05", profileOptions.gpioMappingsSets[i].pins[5]);
        writePinDoc(i, "pin06", profileOptions.gpioMappingsSets[i].pins[6]);
        writePinDoc(i, "pin07", profileOptions.gpioMappingsSets[i].pins[7]);
        writePinDoc(i, "pin08", profileOptions.gpioMappingsSets[i].pins[8]);
        writePinDoc(i, "pin09", profileOptions.gpioMappingsSets[i].pins[9]);
        writePinDoc(i, "pin10", profileOptions.gpioMappingsSets[i].pins[10]);
        writePinDoc(i, "pin11", profileOptions.gpioMappingsSets[i].pins[11]);
        writePinDoc(i, "pin12", profileOptions.gpioMappingsSets[i].pins[12]);
        writePinDoc(i, "pin13", profileOptions.gpioMappingsSets[i].pins[13]);
        writePinDoc(i, "pin14", profileOptions.gpioMappingsSets[i].pins[14]);
        writePinDoc(i, "pin15", profileOptions.gpioMappingsSets[i].pins[15]);
        writePinDoc(i, "pin16", profileOptions.gpioMappingsSets[i].pins[16]);
        writePinDoc(i, "pin17", profileOptions.gpioMappingsSets[i].pins[17]);
        writePinDoc(i, "pin18", profileOptions.gpioMappingsSets[i].pins[18]);
        writePinDoc(i, "pin19", profileOptions.gpioMappingsSets[i].pins[19]);
        writePinDoc(i, "pin20", profileOptions.gpioMappingsSets[i].pins[20]);
        writePinDoc(i, "pin21", profileOptions.gpioMappingsSets[i].pins[21]);
        writePinDoc(i, "pin22", profileOptions.gpioMappingsSets[i].pins[22]);
        writePinDoc(i, "pin23", profileOptions.gpioMappingsSets[i].pins[23]);
        writePinDoc(i, "pin24", profileOptions.gpioMappingsSets[i].pins[24]);
        writePinDoc(i, "pin25", profileOptions.gpioMappingsSets[i].pins[25]);
        writePinDoc(i, "pin26", profileOptions.gpioMappingsSets[i].pins[26]);
        writePinDoc(i, "pin27", profileOptions.gpioMappingsSets[i].pins[27]);
        writePinDoc(i, "pin28", profileOptions.gpioMappingsSets[i].pins[28]);
        writePinDoc(i, "pin29", profileOptions.gpioMappingsSets[i].pins[29]);
        writeDoc(doc, "alternativePinMappings", i, "profileLabel", profileOptions.gpioMappingsSets[i].profileLabel);
        doc["alternativePinMappings"][i]["enabled"] = profileOptions.gpioMappingsSets[i].enabled;
    }

    return serialize_json(doc);
}

std::string setGamepadOptions()
{
    DynamicJsonDocument doc = get_post_data();

    GamepadOptions& gamepadOptions = Storage::getInstance().getGamepadOptions();

    readDoc(gamepadOptions.dpadMode, doc, "dpadMode");
    readDoc(gamepadOptions.inputMode, doc, "inputMode");
    readDoc(gamepadOptions.socdMode, doc, "socdMode");
    readDoc(gamepadOptions.switchTpShareForDs4, doc, "switchTpShareForDs4");
    readDoc(gamepadOptions.lockHotkeys, doc, "lockHotkeys");
    readDoc(gamepadOptions.fourWayMode, doc, "fourWayMode");
    readDoc(gamepadOptions.profileNumber, doc, "profileNumber");
    readDoc(gamepadOptions.debounceDelay, doc, "debounceDelay");
    readDoc(gamepadOptions.inputModeB1, doc, "inputModeB1");
    readDoc(gamepadOptions.inputModeB2, doc, "inputModeB2");
    readDoc(gamepadOptions.inputModeB3, doc, "inputModeB3");
    readDoc(gamepadOptions.inputModeB4, doc, "inputModeB4");
    readDoc(gamepadOptions.inputModeL1, doc, "inputModeL1");
    readDoc(gamepadOptions.inputModeL2, doc, "inputModeL2");
    readDoc(gamepadOptions.inputModeR1, doc, "inputModeR1");
    readDoc(gamepadOptions.inputModeR2, doc, "inputModeR2");
    readDoc(gamepadOptions.ps4AuthType, doc, "ps4AuthType");
    readDoc(gamepadOptions.ps5AuthType, doc, "ps5AuthType");
    readDoc(gamepadOptions.xinputAuthType, doc, "xinputAuthType");
    readDoc(gamepadOptions.ps4ControllerIDMode, doc, "ps4ControllerIDMode");
    readDoc(gamepadOptions.usbDescOverride, doc, "usbDescOverride");
    readDoc(gamepadOptions.miniMenuGamepadInput, doc, "miniMenuGamepadInput");
    // Copy USB descriptor strings
    size_t strSize = sizeof(gamepadOptions.usbDescManufacturer);
    strncpy(gamepadOptions.usbDescManufacturer, doc["usbDescManufacturer"], strSize - 1);
    gamepadOptions.usbDescManufacturer[strSize - 1] = '\0';
    strSize = sizeof(gamepadOptions.usbDescProduct);
    strncpy(gamepadOptions.usbDescProduct, doc["usbDescProduct"], strSize - 1);
    gamepadOptions.usbDescProduct[strSize - 1] = '\0';
    strSize = sizeof(gamepadOptions.usbDescVersion);
    strncpy(gamepadOptions.usbDescVersion, doc["usbDescVersion"], strSize - 1);
    gamepadOptions.usbDescVersion[strSize - 1] = '\0';
    readDoc(gamepadOptions.usbOverrideID, doc, "usbOverrideID");
    readDoc(gamepadOptions.usbVendorID, doc, "usbVendorID");
    readDoc(gamepadOptions.usbProductID, doc, "usbProductID");


    HotkeyOptions& hotkeyOptions = Storage::getInstance().getHotkeyOptions();
    save_hotkey(&hotkeyOptions.hotkey01, doc, "hotkey01");
    save_hotkey(&hotkeyOptions.hotkey02, doc, "hotkey02");
    save_hotkey(&hotkeyOptions.hotkey03, doc, "hotkey03");
    save_hotkey(&hotkeyOptions.hotkey04, doc, "hotkey04");
    save_hotkey(&hotkeyOptions.hotkey05, doc, "hotkey05");
    save_hotkey(&hotkeyOptions.hotkey06, doc, "hotkey06");
    save_hotkey(&hotkeyOptions.hotkey07, doc, "hotkey07");
    save_hotkey(&hotkeyOptions.hotkey08, doc, "hotkey08");
    save_hotkey(&hotkeyOptions.hotkey09, doc, "hotkey09");
    save_hotkey(&hotkeyOptions.hotkey10, doc, "hotkey10");
    save_hotkey(&hotkeyOptions.hotkey11, doc, "hotkey11");
    save_hotkey(&hotkeyOptions.hotkey12, doc, "hotkey12");
    save_hotkey(&hotkeyOptions.hotkey13, doc, "hotkey13");
    save_hotkey(&hotkeyOptions.hotkey14, doc, "hotkey14");
    save_hotkey(&hotkeyOptions.hotkey15, doc, "hotkey15");
    save_hotkey(&hotkeyOptions.hotkey16, doc, "hotkey16");

    ForcedSetupOptions& forcedSetupOptions = Storage::getInstance().getForcedSetupOptions();
    readDoc(forcedSetupOptions.mode, doc, "forcedSetupMode");

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return serialize_json(doc);
}

std::string getGamepadOptions()
{
    const size_t capacity = JSON_OBJECT_SIZE(500);
    DynamicJsonDocument doc(capacity);

    GamepadOptions& gamepadOptions = Storage::getInstance().getGamepadOptions();
    writeDoc(doc, "dpadMode", gamepadOptions.dpadMode);
    writeDoc(doc, "inputMode", gamepadOptions.inputMode);
    writeDoc(doc, "socdMode", gamepadOptions.socdMode);
    writeDoc(doc, "switchTpShareForDs4", gamepadOptions.switchTpShareForDs4 ? 1 : 0);
    writeDoc(doc, "lockHotkeys", gamepadOptions.lockHotkeys ? 1 : 0);
    writeDoc(doc, "fourWayMode", gamepadOptions.fourWayMode ? 1 : 0);
    writeDoc(doc, "profileNumber", gamepadOptions.profileNumber);
    writeDoc(doc, "debounceDelay", gamepadOptions.debounceDelay);
    writeDoc(doc, "inputModeB1", gamepadOptions.inputModeB1);
    writeDoc(doc, "inputModeB2", gamepadOptions.inputModeB2);
    writeDoc(doc, "inputModeB3", gamepadOptions.inputModeB3);
    writeDoc(doc, "inputModeB4", gamepadOptions.inputModeB4);
    writeDoc(doc, "inputModeL1", gamepadOptions.inputModeL1);
    writeDoc(doc, "inputModeL2", gamepadOptions.inputModeL2);
    writeDoc(doc, "inputModeR1", gamepadOptions.inputModeR1);
    writeDoc(doc, "inputModeR2", gamepadOptions.inputModeR2);
    writeDoc(doc, "ps4AuthType", gamepadOptions.ps4AuthType);
    writeDoc(doc, "ps5AuthType", gamepadOptions.ps5AuthType);
    writeDoc(doc, "xinputAuthType", gamepadOptions.xinputAuthType);
    writeDoc(doc, "ps4ControllerIDMode", gamepadOptions.ps4ControllerIDMode);
    writeDoc(doc, "usbDescOverride", gamepadOptions.usbDescOverride);
    writeDoc(doc, "usbDescManufacturer", gamepadOptions.usbDescManufacturer);
    writeDoc(doc, "usbDescProduct", gamepadOptions.usbDescProduct);
    writeDoc(doc, "usbDescVersion", gamepadOptions.usbDescVersion);
    writeDoc(doc, "usbOverrideID", gamepadOptions.usbOverrideID);
    writeDoc(doc, "miniMenuGamepadInput", gamepadOptions.miniMenuGamepadInput);
    // Write USB Vendor ID and Product ID as 4 character hex strings with 0 padding
    char usbVendorStr[5];
    snprintf(usbVendorStr, 5, "%04X", gamepadOptions.usbVendorID);
    writeDoc(doc, "usbVendorID", usbVendorStr);
    char usbProductStr[5];
    snprintf(usbProductStr, 5, "%04X", gamepadOptions.usbProductID);
    writeDoc(doc, "usbProductID", usbProductStr);
    writeDoc(doc, "fnButtonPin", -1);
    GpioMappingInfo* gpioMappings = Storage::getInstance().getGpioMappings().pins;
    for (unsigned int pin = 0; pin < NUM_BANK0_GPIOS; pin++) {
        if (gpioMappings[pin].action == GpioAction::BUTTON_PRESS_FN) {
            writeDoc(doc, "fnButtonPin", pin);
        }
    }

    HotkeyOptions& hotkeyOptions = Storage::getInstance().getHotkeyOptions();
    load_hotkey(&hotkeyOptions.hotkey01, doc, "hotkey01");
    load_hotkey(&hotkeyOptions.hotkey02, doc, "hotkey02");
    load_hotkey(&hotkeyOptions.hotkey03, doc, "hotkey03");
    load_hotkey(&hotkeyOptions.hotkey04, doc, "hotkey04");
    load_hotkey(&hotkeyOptions.hotkey05, doc, "hotkey05");
    load_hotkey(&hotkeyOptions.hotkey06, doc, "hotkey06");
    load_hotkey(&hotkeyOptions.hotkey07, doc, "hotkey07");
    load_hotkey(&hotkeyOptions.hotkey08, doc, "hotkey08");
    load_hotkey(&hotkeyOptions.hotkey09, doc, "hotkey09");
    load_hotkey(&hotkeyOptions.hotkey10, doc, "hotkey10");
    load_hotkey(&hotkeyOptions.hotkey11, doc, "hotkey11");
    load_hotkey(&hotkeyOptions.hotkey12, doc, "hotkey12");
    load_hotkey(&hotkeyOptions.hotkey13, doc, "hotkey13");
    load_hotkey(&hotkeyOptions.hotkey14, doc, "hotkey14");
    load_hotkey(&hotkeyOptions.hotkey15, doc, "hotkey15");
    load_hotkey(&hotkeyOptions.hotkey16, doc, "hotkey16");

    ForcedSetupOptions& forcedSetupOptions = Storage::getInstance().getForcedSetupOptions();
    writeDoc(doc, "forcedSetupMode", forcedSetupOptions.mode);
    return serialize_json(doc);
}

std::string setLedOptions()
{
    DynamicJsonDocument doc = get_post_data();

    const auto readIndex = [&](int32_t& var, const char* key0, const char* key1)
    {
        var = -1;
        if (hasValue(doc, key0, key1))
        {
            readDoc(var, doc, key0, key1);
        }
    };

    LEDOptions& ledOptions = Storage::getInstance().getLedOptions();
    docToPin(ledOptions.dataPin, doc, "dataPin");
    readDoc(ledOptions.ledFormat, doc, "ledFormat");
    readDoc(ledOptions.ledLayout, doc, "ledLayout");
    readDoc(ledOptions.ledsPerButton, doc, "ledsPerButton");
    readDoc(ledOptions.brightnessMaximum, doc, "brightnessMaximum");
    readDoc(ledOptions.brightnessSteps, doc, "brightnessSteps");
    readDoc(ledOptions.turnOffWhenSuspended, doc, "turnOffWhenSuspended");
    readIndex(ledOptions.indexUp, "ledButtonMap", "Up");
    readIndex(ledOptions.indexDown, "ledButtonMap", "Down");
    readIndex(ledOptions.indexLeft, "ledButtonMap", "Left");
    readIndex(ledOptions.indexRight, "ledButtonMap", "Right");
    readIndex(ledOptions.indexB1, "ledButtonMap", "B1");
    readIndex(ledOptions.indexB2, "ledButtonMap", "B2");
    readIndex(ledOptions.indexB3, "ledButtonMap", "B3");
    readIndex(ledOptions.indexB4, "ledButtonMap", "B4");
    readIndex(ledOptions.indexL1, "ledButtonMap", "L1");
    readIndex(ledOptions.indexR1, "ledButtonMap", "R1");
    readIndex(ledOptions.indexL2, "ledButtonMap", "L2");
    readIndex(ledOptions.indexR2, "ledButtonMap", "R2");
    readIndex(ledOptions.indexS1, "ledButtonMap", "S1");
    readIndex(ledOptions.indexS2, "ledButtonMap", "S2");
    readIndex(ledOptions.indexL3, "ledButtonMap", "L3");
    readIndex(ledOptions.indexR3, "ledButtonMap", "R3");
    readIndex(ledOptions.indexA1, "ledButtonMap", "A1");
    readIndex(ledOptions.indexA2, "ledButtonMap", "A2");
    readDoc(ledOptions.pledType, doc, "pledType");
    docToPin(ledOptions.pledPin1, doc, "pledPin1");
    docToPin(ledOptions.pledPin2, doc, "pledPin2");
    docToPin(ledOptions.pledPin3, doc, "pledPin3");
    docToPin(ledOptions.pledPin4, doc, "pledPin4");
    readDoc(ledOptions.pledIndex1, doc, "pledIndex1");
    readDoc(ledOptions.pledIndex2, doc, "pledIndex2");
    readDoc(ledOptions.pledIndex3, doc, "pledIndex3");
    readDoc(ledOptions.pledIndex4, doc, "pledIndex4");
    readDoc(ledOptions.pledColor, doc, "pledColor");
    readDoc(ledOptions.caseRGBType, doc, "caseRGBType");
    readDoc(ledOptions.caseRGBIndex, doc, "caseRGBIndex");
    readDoc(ledOptions.caseRGBCount, doc, "caseRGBCount");

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));
    return serialize_json(doc);
}

std::string getLedOptions()
{
    const size_t capacity = JSON_OBJECT_SIZE(500);
    DynamicJsonDocument doc(capacity);
    const LEDOptions& ledOptions = Storage::getInstance().getLedOptions();
    writeDoc(doc, "dataPin", cleanPin(ledOptions.dataPin));
    writeDoc(doc, "ledFormat", ledOptions.ledFormat);
    writeDoc(doc, "ledLayout", ledOptions.ledLayout);
    writeDoc(doc, "ledsPerButton", ledOptions.ledsPerButton);
    writeDoc(doc, "brightnessMaximum", ledOptions.brightnessMaximum);
    writeDoc(doc, "brightnessSteps", ledOptions.brightnessSteps);
    writeDoc(doc, "turnOffWhenSuspended", ledOptions.turnOffWhenSuspended);

    const auto writeIndex = [&](const char* key0, const char* key1, int var)
    {
        if (var < 0)
        {
            writeDoc(doc, key0, key1, nullptr);
        }
        else
        {
            writeDoc(doc, key0, key1, var);
        }
    };
    writeIndex("ledButtonMap", "Up", ledOptions.indexUp);
    writeIndex("ledButtonMap", "Down", ledOptions.indexDown);
    writeIndex("ledButtonMap", "Left", ledOptions.indexLeft);
    writeIndex("ledButtonMap", "Right", ledOptions.indexRight);
    writeIndex("ledButtonMap", "B1", ledOptions.indexB1);
    writeIndex("ledButtonMap", "B2", ledOptions.indexB2);
    writeIndex("ledButtonMap", "B3", ledOptions.indexB3);
    writeIndex("ledButtonMap", "B4", ledOptions.indexB4);
    writeIndex("ledButtonMap", "L1", ledOptions.indexL1);
    writeIndex("ledButtonMap", "R1", ledOptions.indexR1);
    writeIndex("ledButtonMap", "L2", ledOptions.indexL2);
    writeIndex("ledButtonMap", "R2", ledOptions.indexR2);
    writeIndex("ledButtonMap", "S1", ledOptions.indexS1);
    writeIndex("ledButtonMap", "S2", ledOptions.indexS2);
    writeIndex("ledButtonMap", "L3", ledOptions.indexL3);
    writeIndex("ledButtonMap", "R3", ledOptions.indexR3);
    writeIndex("ledButtonMap", "A1", ledOptions.indexA1);
    writeIndex("ledButtonMap", "A2", ledOptions.indexA2);
    writeDoc(doc, "pledType", ledOptions.pledType);
    writeDoc(doc, "pledPin1", ledOptions.pledPin1);
    writeDoc(doc, "pledPin2", ledOptions.pledPin2);
    writeDoc(doc, "pledPin3", ledOptions.pledPin3);
    writeDoc(doc, "pledPin4", ledOptions.pledPin4);
    writeDoc(doc, "pledIndex1", ledOptions.pledIndex1);
    writeDoc(doc, "pledIndex2", ledOptions.pledIndex2);
    writeDoc(doc, "pledIndex3", ledOptions.pledIndex3);
    writeDoc(doc, "pledIndex4", ledOptions.pledIndex4);
    writeDoc(doc, "pledColor", ((RGB)ledOptions.pledColor).value(LED_FORMAT_RGB));



    return serialize_json(doc);
}

std::string getButtonLayoutDefs()
{
    const size_t capacity = JSON_OBJECT_SIZE(500);
    DynamicJsonDocument doc(capacity);
    uint16_t layoutCtr = 0;

    for (layoutCtr = _ButtonLayout_MIN; layoutCtr < _ButtonLayout_ARRAYSIZE; layoutCtr++) {
        LayoutManager::LayoutList leftLayout = LayoutManager::getInstance().getLeftLayout((ButtonLayout)layoutCtr);
        if ((leftLayout.size() > 0) || (layoutCtr == ButtonLayout::BUTTON_LAYOUT_BLANKA)) writeDoc(doc, "buttonLayout", LayoutManager::getInstance().getButtonLayoutName((ButtonLayout)layoutCtr), layoutCtr);
    }

    for (layoutCtr = _ButtonLayoutRight_MIN; layoutCtr < _ButtonLayoutRight_ARRAYSIZE; layoutCtr++) {
        LayoutManager::LayoutList rightLayout = LayoutManager::getInstance().getRightLayout((ButtonLayoutRight)layoutCtr);
        if ((rightLayout.size() > 0) || (layoutCtr == ButtonLayoutRight::BUTTON_LAYOUT_BLANKB)) writeDoc(doc, "buttonLayoutRight", LayoutManager::getInstance().getButtonLayoutRightName((ButtonLayoutRight)layoutCtr), layoutCtr);
    }

    return serialize_json(doc);
}

std::string getButtonLayouts()
{
    const size_t capacity = JSON_OBJECT_SIZE(500);
    DynamicJsonDocument doc(capacity);
    const LEDOptions& ledOptions = Storage::getInstance().getLedOptions();
    const DisplayOptions& displayOptions = Storage::getInstance().getDisplayOptions();
    uint16_t elementCtr = 0;

    LayoutManager::LayoutList layoutA = LayoutManager::getInstance().getLayoutA();
    LayoutManager::LayoutList layoutB = LayoutManager::getInstance().getLayoutB();

    writeDoc(doc, "ledLayout", "id", ledOptions.ledLayout);
    writeDoc(doc, "ledLayout", "indexUp", ledOptions.indexUp);
    writeDoc(doc, "ledLayout", "indexDown", ledOptions.indexDown);
    writeDoc(doc, "ledLayout", "indexLeft", ledOptions.indexLeft);
    writeDoc(doc, "ledLayout", "indexRight", ledOptions.indexRight);
    writeDoc(doc, "ledLayout", "indexB1", ledOptions.indexB1);
    writeDoc(doc, "ledLayout", "indexB2", ledOptions.indexB2);
    writeDoc(doc, "ledLayout", "indexB3", ledOptions.indexB3);
    writeDoc(doc, "ledLayout", "indexB4", ledOptions.indexB4);
    writeDoc(doc, "ledLayout", "indexL1", ledOptions.indexL1);
    writeDoc(doc, "ledLayout", "indexR1", ledOptions.indexR1);
    writeDoc(doc, "ledLayout", "indexL2", ledOptions.indexL2);
    writeDoc(doc, "ledLayout", "indexR2", ledOptions.indexR2);
    writeDoc(doc, "ledLayout", "indexS1", ledOptions.indexS1);
    writeDoc(doc, "ledLayout", "indexS2", ledOptions.indexS2);
    writeDoc(doc, "ledLayout", "indexL3", ledOptions.indexL3);
    writeDoc(doc, "ledLayout", "indexR3", ledOptions.indexR3);
    writeDoc(doc, "ledLayout", "indexA1", ledOptions.indexA1);
    writeDoc(doc, "ledLayout", "indexA2", ledOptions.indexA2);

    writeDoc(doc, "displayLayouts", "buttonLayoutId", displayOptions.buttonLayout);
    for (elementCtr = 0; elementCtr < layoutA.size(); elementCtr++) {
        const size_t elementSize = JSON_OBJECT_SIZE(12);
        DynamicJsonDocument ele(elementSize);

        writeDoc(ele, "elementType", layoutA[elementCtr].elementType);
        writeDoc(ele, "parameters", "x1", layoutA[elementCtr].parameters.x1);
        writeDoc(ele, "parameters", "y1", layoutA[elementCtr].parameters.y1);
        writeDoc(ele, "parameters", "x2", layoutA[elementCtr].parameters.x2);
        writeDoc(ele, "parameters", "y2", layoutA[elementCtr].parameters.y2);
        writeDoc(ele, "parameters", "stroke", layoutA[elementCtr].parameters.stroke);
        writeDoc(ele, "parameters", "fill", layoutA[elementCtr].parameters.fill);
        writeDoc(ele, "parameters", "value", layoutA[elementCtr].parameters.value);
        writeDoc(ele, "parameters", "shape", layoutA[elementCtr].parameters.shape);
        writeDoc(ele, "parameters", "angleStart", layoutA[elementCtr].parameters.angleStart);
        writeDoc(ele, "parameters", "angleEnd", layoutA[elementCtr].parameters.angleEnd);
        writeDoc(ele, "parameters", "closed", layoutA[elementCtr].parameters.closed);
        writeDoc(doc, "displayLayouts", "buttonLayout", std::to_string(elementCtr), ele);
    }

    writeDoc(doc, "displayLayouts", "buttonLayoutRightId", displayOptions.buttonLayoutRight);
    for (elementCtr = 0; elementCtr < layoutB.size(); elementCtr++) {
        const size_t elementSize = JSON_OBJECT_SIZE(12);
        DynamicJsonDocument ele(elementSize);

        writeDoc(ele, "elementType", layoutB[elementCtr].elementType);
        writeDoc(ele, "parameters", "x1", layoutB[elementCtr].parameters.x1);
        writeDoc(ele, "parameters", "y1", layoutB[elementCtr].parameters.y1);
        writeDoc(ele, "parameters", "x2", layoutB[elementCtr].parameters.x2);
        writeDoc(ele, "parameters", "y2", layoutB[elementCtr].parameters.y2);
        writeDoc(ele, "parameters", "stroke", layoutB[elementCtr].parameters.stroke);
        writeDoc(ele, "parameters", "fill", layoutB[elementCtr].parameters.fill);
        writeDoc(ele, "parameters", "value", layoutB[elementCtr].parameters.value);
        writeDoc(ele, "parameters", "shape", layoutB[elementCtr].parameters.shape);
        writeDoc(ele, "parameters", "angleStart", layoutB[elementCtr].parameters.angleStart);
        writeDoc(ele, "parameters", "angleEnd", layoutB[elementCtr].parameters.angleEnd);
        writeDoc(ele, "parameters", "closed", layoutB[elementCtr].parameters.closed);
        writeDoc(doc, "displayLayouts", "buttonLayoutRight", std::to_string(elementCtr), ele);
    }

    return serialize_json(doc);
}

std::string setLightsDataOptions()
{
    DynamicJsonDocument doc = get_post_data();

    LEDOptions& options = Storage::getInstance().getLedOptions();

    JsonObject docJson = doc.as<JsonObject>();
    JsonObject AnimOptions = docJson["LightData"];
    JsonArray lightsList = AnimOptions["Lights"];
    options.lightDataSize = 0;
    for (JsonObject light : lightsList)
    {
        int thisEntryIndex = options.lightDataSize * 6;
        options.lightData.bytes[thisEntryIndex] = light["firstLedIndex"].as<uint8_t>();
        options.lightData.bytes[thisEntryIndex+1] = light["numLedsOnLight"].as<uint8_t>();
        options.lightData.bytes[thisEntryIndex+2] = light["xCoord"].as<uint8_t>();
        options.lightData.bytes[thisEntryIndex+3] = light["yCoord"].as<uint8_t>();
        options.lightData.bytes[thisEntryIndex+4] = light["GPIOPinorCaseChainIndex"].as<uint8_t>();
        options.lightData.bytes[thisEntryIndex+5] = (LightType)(light["lightType"].as<uint8_t>());

        options.lightDataSize++;
        options.lightData.size = options.lightDataSize * 6;

        if(options.lightDataSize >= 100) //600 bytes total, 6 elements per light. 100 max lights
            break;
    }

    NeoPicoLEDAddon::RestartLedSystem();

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));
    return serialize_json(doc);
}

std::string getLightsDataOptions()
{
    DynamicJsonDocument doc(LWIP_HTTPD_POST_MAX_PAYLOAD_LEN);
    const LEDOptions& options = Storage::getInstance().getLedOptions();

    JsonObject LedOptions = doc.createNestedObject("LightData");
    JsonArray lightsList = LedOptions.createNestedArray("Lights");
    for (int lightsIndex = 0; lightsIndex < options.lightDataSize; ++lightsIndex)
    {
        int thisEntryIndex = lightsIndex * 6;
        JsonObject light = lightsList.createNestedObject();
        light["firstLedIndex"] = options.lightData.bytes[thisEntryIndex];
        light["numLedsOnLight"] = options.lightData.bytes[thisEntryIndex+1];
        light["xCoord"] = options.lightData.bytes[thisEntryIndex+2];
        light["yCoord"] = options.lightData.bytes[thisEntryIndex+3];
        light["GPIOPinorCaseChainIndex"] = options.lightData.bytes[thisEntryIndex+4];
        light["lightType"] = options.lightData.bytes[thisEntryIndex+5];
    }

    return serialize_json(doc);
}

void helperGetProfileFromJsonObject(AnimationProfile* Profile, JsonObject* JsonData)
{
    Profile->bEnabled = (*JsonData)["bEnabled"].as<bool>();
    if(Profile->baseNonPressedEffect != (AnimationNonPressedEffects)((*JsonData)["baseNonPressedEffect"].as<uint32_t>()))
    {
        Profile->baseNonPressedEffect = (AnimationNonPressedEffects)((*JsonData)["baseNonPressedEffect"].as<uint32_t>());
        Profile->baseCycleTime = 0;
    }
    if(Profile->basePressedEffect != (AnimationPressedEffects)((*JsonData)["basePressedEffect"].as<uint32_t>()))
    {
        Profile->basePressedEffect = (AnimationPressedEffects)((*JsonData)["basePressedEffect"].as<uint32_t>());
        Profile->basePressedCycleTime = 0;
    }
    Profile->buttonPressHoldTimeInMs = (*JsonData)["buttonPressHoldTimeInMs"].as<uint32_t>();
    Profile->buttonPressFadeOutTimeInMs = (*JsonData)["buttonPressFadeOutTimeInMs"].as<uint32_t>();
    Profile->nonPressedSpecialColor = (*JsonData)["nonPressedSpecialColor"].as<uint32_t>();
    Profile->bUseCaseLightsInSpecialMoves = (*JsonData)["bUseCaseLightsInSpecialMoves"].as<bool>();
    Profile->bUseCaseLightsInPressedAnimations = (*JsonData)["bUseCaseLightsInPressedAnimations"].as<bool>();
    Profile->baseCaseEffect = (AnimationNonPressedEffects)((*JsonData)["baseCaseEffect"].as<uint32_t>());
    Profile->pressedSpecialColor = (*JsonData)["pressedSpecialColor"].as<uint32_t>();

    JsonArray notPressedStaticColorsList = (*JsonData)["notPressedStaticColors"];
    Profile->notPressedStaticColors_count = 0;
    for(unsigned int packedPinIndex = 0; packedPinIndex < (NUM_BANK0_GPIOS/4)+1; ++packedPinIndex)
    {
        unsigned int pinIndex = packedPinIndex * 4;
        if(pinIndex < notPressedStaticColorsList.size())
            Profile->notPressedStaticColors[packedPinIndex] = notPressedStaticColorsList[pinIndex].as<uint32_t>() & 0xFF;
        else
            break;
        if(pinIndex+1 < notPressedStaticColorsList.size())
            Profile->notPressedStaticColors[packedPinIndex] += ((notPressedStaticColorsList[pinIndex+1].as<uint32_t>() & 0xFF) << 8);
        if(pinIndex+2 < notPressedStaticColorsList.size())
            Profile->notPressedStaticColors[packedPinIndex] += ((notPressedStaticColorsList[pinIndex+2].as<uint32_t>() & 0xFF) << 16);
        if(pinIndex+3 < notPressedStaticColorsList.size())
            Profile->notPressedStaticColors[packedPinIndex] += ((notPressedStaticColorsList[pinIndex+3].as<uint32_t>() & 0xFF) << 24);
        Profile->notPressedStaticColors_count = packedPinIndex+1;
    }

    JsonArray pressedStaticColorsList = (*JsonData)["pressedStaticColors"];
    Profile->pressedStaticColors_count = 0;
    for(unsigned int packedPinIndex = 0; packedPinIndex < (NUM_BANK0_GPIOS/4)+1; ++packedPinIndex)
    {
        unsigned int pinIndex = packedPinIndex * 4;
        if(pinIndex < pressedStaticColorsList.size())
            Profile->pressedStaticColors[packedPinIndex] = pressedStaticColorsList[pinIndex].as<uint32_t>() & 0xFF;
        else
            break;
        if(pinIndex+1 < pressedStaticColorsList.size())
            Profile->pressedStaticColors[packedPinIndex] += ((pressedStaticColorsList[pinIndex+1].as<uint32_t>() & 0xFF) << 8);
        if(pinIndex+2 < pressedStaticColorsList.size())
            Profile->pressedStaticColors[packedPinIndex] += ((pressedStaticColorsList[pinIndex+2].as<uint32_t>() & 0xFF) << 16);
        if(pinIndex+3 < pressedStaticColorsList.size())
            Profile->pressedStaticColors[packedPinIndex] += ((pressedStaticColorsList[pinIndex+3].as<uint32_t>() & 0xFF) << 24);
        Profile->pressedStaticColors_count = packedPinIndex+1;
    }

    JsonArray caseStaticColorsList = (*JsonData)["caseStaticColors"];
    Profile->caseStaticColors_count = 0;
    for(unsigned int packedPinIndex = 0; packedPinIndex < (NUM_BANK0_GPIOS/4)+1; ++packedPinIndex)
    {
        unsigned int pinIndex = packedPinIndex * 4;
        if(pinIndex < caseStaticColorsList.size())
            Profile->caseStaticColors[packedPinIndex] = caseStaticColorsList[pinIndex].as<uint32_t>() & 0xFF;
        else
            break;
        if(pinIndex+1 < caseStaticColorsList.size())
            Profile->caseStaticColors[packedPinIndex] += ((caseStaticColorsList[pinIndex+1].as<uint32_t>() & 0xFF) << 8);
        if(pinIndex+2 < caseStaticColorsList.size())
            Profile->caseStaticColors[packedPinIndex] += ((caseStaticColorsList[pinIndex+2].as<uint32_t>() & 0xFF) << 16);
        if(pinIndex+3 < caseStaticColorsList.size())
            Profile->caseStaticColors[packedPinIndex] += ((caseStaticColorsList[pinIndex+3].as<uint32_t>() & 0xFF) << 24);
        Profile->caseStaticColors_count = packedPinIndex+1;
    }
}

std::string setAnimationButtonTestMode()
{
    DynamicJsonDocument doc = get_post_data();

    JsonObject docJson = doc.as<JsonObject>();
    JsonObject testOptions = docJson["TestData"];

    AnimationStationTestMode testMode = (AnimationStationTestMode)(testOptions["testMode"].as<uint32_t>());

    AnimationProfile testAnimProfile;
    if(testMode == AnimationStationTestMode::AnimationStation_TestModeProfilePreview)
    {
        JsonObject testProfile = testOptions["testProfile"];
        helperGetProfileFromJsonObject(&testAnimProfile, &testProfile);
    }

    AnimationStation::SetTestMode(testMode, &testAnimProfile);

    return serialize_json(doc);
}

std::string setAnimationButtonTestState()
{
    DynamicJsonDocument doc = get_post_data();

    JsonObject docJson = doc.as<JsonObject>();
    JsonObject testOptions = docJson["TestLight"];
    int testButton = testOptions["testID"].as<uint32_t>();
    bool testIsCaseLight = testOptions["testIsCaseLight"].as<bool>();
    
    AnimationStation::SetTestPinState(testButton, testIsCaseLight);

    return serialize_json(doc);
}

std::string setAnimationProtoOptions()
{
    DynamicJsonDocument doc = get_post_data();

    AnimationOptions& options = Storage::getInstance().getAnimationOptions();

    JsonObject docJson = doc.as<JsonObject>();
    JsonObject AnimOptions = docJson["AnimationOptions"];

    options.brightness = AnimOptions["brightness"].as<uint32_t>();
    options.baseProfileIndex = AnimOptions["baseProfileIndex"].as<uint32_t>();
    JsonArray customColorsList = AnimOptions["customColors"];
    options.customColors_count = 0;
    for(unsigned int customColorsIndex = 0; customColorsIndex < customColorsList.size() && customColorsIndex < MAX_CUSTOM_COLORS; ++customColorsIndex)
    {
        options.customColors[customColorsIndex] = customColorsList[customColorsIndex];
        options.customColors_count = customColorsIndex+1;
    }

    JsonArray profilesList = AnimOptions["profiles"];
    int profilesIndex = 0;
    options.profiles_count = 0;
    for (JsonObject profile : profilesList)
    {
        helperGetProfileFromJsonObject(&(options.profiles[profilesIndex]), &profile);

        options.profiles_count = profilesIndex+1;

        if (++profilesIndex >= MAX_ANIMATION_PROFILES)
            break;
    }

    NeoPicoLEDAddon::RestartLedSystem();

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));
    return serialize_json(doc);
}

std::string getAnimationProtoOptions()
{
    DynamicJsonDocument doc(LWIP_HTTPD_POST_MAX_PAYLOAD_LEN);
    const AnimationOptions& options = Storage::getInstance().getAnimationOptions();
    const LEDOptions& ledOptions = Storage::getInstance().getLedOptions();

    uint32_t checkedBrightness = std::clamp<uint32_t>(options.brightness, 0, ledOptions.brightnessSteps);

    JsonObject AnimOptions = doc.createNestedObject("AnimationOptions");
    AnimOptions["brightness"] = checkedBrightness;
    AnimOptions["baseProfileIndex"] = options.baseProfileIndex;
    JsonArray customColorsList = AnimOptions.createNestedArray("customColors");
    for (int customColorsIndex = 0; customColorsIndex < options.customColors_count; ++customColorsIndex)
    {
        customColorsList.add(options.customColors[customColorsIndex]);
    }

    JsonArray profileList = AnimOptions.createNestedArray("profiles");
    for (int profilesIndex = 0; profilesIndex < options.profiles_count; ++profilesIndex)
    {
        JsonObject profile = profileList.createNestedObject();
        profile["bEnabled"] = options.profiles[profilesIndex].bEnabled ? 1 : 0;
        profile["baseNonPressedEffect"] = options.profiles[profilesIndex].baseNonPressedEffect;
        profile["basePressedEffect"] = options.profiles[profilesIndex].basePressedEffect;
        profile["buttonPressHoldTimeInMs"] = options.profiles[profilesIndex].buttonPressHoldTimeInMs;
        profile["buttonPressFadeOutTimeInMs"] = options.profiles[profilesIndex].buttonPressFadeOutTimeInMs;
        profile["nonPressedSpecialColor"] = options.profiles[profilesIndex].nonPressedSpecialColor;
        profile["bUseCaseLightsInSpecialMoves"] = options.profiles[profilesIndex].bUseCaseLightsInSpecialMoves ? 1 : 0;
        profile["bUseCaseLightsInPressedAnimations"] = options.profiles[profilesIndex].bUseCaseLightsInPressedAnimations ? 1 : 0;
        profile["baseCaseEffect"] = options.profiles[profilesIndex].baseCaseEffect;
        profile["pressedSpecialColor"] = options.profiles[profilesIndex].pressedSpecialColor;

        JsonArray notPressedStaticColorsList = profile.createNestedArray("notPressedStaticColors");
        for (int notPressedStaticColorsIndex = 0; notPressedStaticColorsIndex < options.profiles[profilesIndex].notPressedStaticColors_count; ++notPressedStaticColorsIndex)
        {
            notPressedStaticColorsList.add(options.profiles[profilesIndex].notPressedStaticColors[notPressedStaticColorsIndex] & 0xFF);
            notPressedStaticColorsList.add((options.profiles[profilesIndex].notPressedStaticColors[notPressedStaticColorsIndex] >> 8) & 0xFF);
            notPressedStaticColorsList.add((options.profiles[profilesIndex].notPressedStaticColors[notPressedStaticColorsIndex] >> 16) & 0xFF);
            notPressedStaticColorsList.add((options.profiles[profilesIndex].notPressedStaticColors[notPressedStaticColorsIndex] >> 24) & 0xFF);
        }
        JsonArray pressedStaticColorsList = profile.createNestedArray("pressedStaticColors");
        for (int pressedStaticColorsIndex = 0; pressedStaticColorsIndex < options.profiles[profilesIndex].pressedStaticColors_count; ++pressedStaticColorsIndex)
        {
            pressedStaticColorsList.add(options.profiles[profilesIndex].pressedStaticColors[pressedStaticColorsIndex] & 0xFF);
            pressedStaticColorsList.add((options.profiles[profilesIndex].pressedStaticColors[pressedStaticColorsIndex] >> 8) & 0xFF);
            pressedStaticColorsList.add((options.profiles[profilesIndex].pressedStaticColors[pressedStaticColorsIndex] >> 16) & 0xFF);
            pressedStaticColorsList.add((options.profiles[profilesIndex].pressedStaticColors[pressedStaticColorsIndex] >> 24) & 0xFF);
        }
        JsonArray caseStaticColorsList = profile.createNestedArray("caseStaticColors");
        for (int caseStaticColorsIndex = 0; caseStaticColorsIndex < options.profiles[profilesIndex].caseStaticColors_count; ++caseStaticColorsIndex)
        {
            caseStaticColorsList.add(options.profiles[profilesIndex].caseStaticColors[caseStaticColorsIndex] & 0xFF);
            caseStaticColorsList.add((options.profiles[profilesIndex].caseStaticColors[caseStaticColorsIndex] >> 8) & 0xFF);
            caseStaticColorsList.add((options.profiles[profilesIndex].caseStaticColors[caseStaticColorsIndex] >> 16) & 0xFF);
            caseStaticColorsList.add((options.profiles[profilesIndex].caseStaticColors[caseStaticColorsIndex] >> 24) & 0xFF);
        }        
    }

    return serialize_json(doc);
}

std::string setPinMappings()
{
    DynamicJsonDocument doc = get_post_data();

    GpioMappings& gpioMappings = Storage::getInstance().getGpioMappings();

    char pinName[6];
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++) {
        snprintf(pinName, 6, "pin%0*d", 2, pin);
        // setting a pin shouldn't change a new existing addon/reserved pin
        if (gpioMappings.pins[pin].action != GpioAction::RESERVED &&
                gpioMappings.pins[pin].action != GpioAction::ASSIGNED_TO_ADDON &&
                (GpioAction)doc[pinName]["action"] != GpioAction::RESERVED &&
                (GpioAction)doc[pinName]["action"] != GpioAction::ASSIGNED_TO_ADDON) {
            gpioMappings.pins[pin].action = (GpioAction)doc[pinName]["action"];
            gpioMappings.pins[pin].customButtonMask = (uint32_t)doc[pinName]["customButtonMask"];
            gpioMappings.pins[pin].customDpadMask = (uint32_t)doc[pinName]["customDpadMask"];
        }
    }
    size_t profileLabelSize = sizeof(gpioMappings.profileLabel);
    strncpy(gpioMappings.profileLabel, doc["profileLabel"], profileLabelSize - 1);
    gpioMappings.profileLabel[profileLabelSize - 1] = '\0';
    gpioMappings.enabled = doc["enabled"];

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return serialize_json(doc);
}

std::string getPinMappings()
{
    const size_t capacity = JSON_OBJECT_SIZE(500);
    DynamicJsonDocument doc(capacity);

    GpioMappings& gpioMappings = Storage::getInstance().getGpioMappings();

    const auto writePinDoc = [&](const char* key, const GpioMappingInfo& value) -> void
    {
        writeDoc(doc, key, "action", value.action);
        writeDoc(doc, key, "customButtonMask", value.customButtonMask);
        writeDoc(doc, key, "customDpadMask", value.customDpadMask);
    };

    writePinDoc("pin00", gpioMappings.pins[0]);
    writePinDoc("pin01", gpioMappings.pins[1]);
    writePinDoc("pin02", gpioMappings.pins[2]);
    writePinDoc("pin03", gpioMappings.pins[3]);
    writePinDoc("pin04", gpioMappings.pins[4]);
    writePinDoc("pin05", gpioMappings.pins[5]);
    writePinDoc("pin06", gpioMappings.pins[6]);
    writePinDoc("pin07", gpioMappings.pins[7]);
    writePinDoc("pin08", gpioMappings.pins[8]);
    writePinDoc("pin09", gpioMappings.pins[9]);
    writePinDoc("pin10", gpioMappings.pins[10]);
    writePinDoc("pin11", gpioMappings.pins[11]);
    writePinDoc("pin12", gpioMappings.pins[12]);
    writePinDoc("pin13", gpioMappings.pins[13]);
    writePinDoc("pin14", gpioMappings.pins[14]);
    writePinDoc("pin15", gpioMappings.pins[15]);
    writePinDoc("pin16", gpioMappings.pins[16]);
    writePinDoc("pin17", gpioMappings.pins[17]);
    writePinDoc("pin18", gpioMappings.pins[18]);
    writePinDoc("pin19", gpioMappings.pins[19]);
    writePinDoc("pin20", gpioMappings.pins[20]);
    writePinDoc("pin21", gpioMappings.pins[21]);
    writePinDoc("pin22", gpioMappings.pins[22]);
    writePinDoc("pin23", gpioMappings.pins[23]);
    writePinDoc("pin24", gpioMappings.pins[24]);
    writePinDoc("pin25", gpioMappings.pins[25]);
    writePinDoc("pin26", gpioMappings.pins[26]);
    writePinDoc("pin27", gpioMappings.pins[27]);
    writePinDoc("pin28", gpioMappings.pins[28]);
    writePinDoc("pin29", gpioMappings.pins[29]);

    writeDoc(doc, "profileLabel", gpioMappings.profileLabel);
    doc["enabled"] = gpioMappings.enabled;

    return serialize_json(doc);
}

std::string setKeyMappings()
{
    DynamicJsonDocument doc = get_post_data();

    KeyboardMapping& keyboardMapping = Storage::getInstance().getKeyboardMapping();

    readDoc(keyboardMapping.keyDpadUp, doc, "Up");
    readDoc(keyboardMapping.keyDpadDown, doc, "Down");
    readDoc(keyboardMapping.keyDpadLeft, doc, "Left");
    readDoc(keyboardMapping.keyDpadRight, doc, "Right");
    readDoc(keyboardMapping.keyButtonB1, doc, "B1");
    readDoc(keyboardMapping.keyButtonB2, doc, "B2");
    readDoc(keyboardMapping.keyButtonB3, doc, "B3");
    readDoc(keyboardMapping.keyButtonB4, doc, "B4");
    readDoc(keyboardMapping.keyButtonL1, doc, "L1");
    readDoc(keyboardMapping.keyButtonR1, doc, "R1");
    readDoc(keyboardMapping.keyButtonL2, doc, "L2");
    readDoc(keyboardMapping.keyButtonR2, doc, "R2");
    readDoc(keyboardMapping.keyButtonS1, doc, "S1");
    readDoc(keyboardMapping.keyButtonS2, doc, "S2");
    readDoc(keyboardMapping.keyButtonL3, doc, "L3");
    readDoc(keyboardMapping.keyButtonR3, doc, "R3");
    readDoc(keyboardMapping.keyButtonA1, doc, "A1");
    readDoc(keyboardMapping.keyButtonA2, doc, "A2");
    readDoc(keyboardMapping.keyButtonA3, doc, "A3");
    readDoc(keyboardMapping.keyButtonA4, doc, "A4");
    readDoc(keyboardMapping.keyButtonE1, doc, "E1");
    readDoc(keyboardMapping.keyButtonE2, doc, "E2");
    readDoc(keyboardMapping.keyButtonE3, doc, "E3");
    readDoc(keyboardMapping.keyButtonE4, doc, "E4");
    readDoc(keyboardMapping.keyButtonE5, doc, "E5");
    readDoc(keyboardMapping.keyButtonE6, doc, "E6");
    readDoc(keyboardMapping.keyButtonE7, doc, "E7");
    readDoc(keyboardMapping.keyButtonE8, doc, "E8");
    readDoc(keyboardMapping.keyButtonE9, doc, "E9");
    readDoc(keyboardMapping.keyButtonE10, doc, "E10");
    readDoc(keyboardMapping.keyButtonE11, doc, "E11");
    readDoc(keyboardMapping.keyButtonE12, doc, "E12");

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return serialize_json(doc);
}

std::string getKeyMappings()
{
    const size_t capacity = JSON_OBJECT_SIZE(100);
    DynamicJsonDocument doc(capacity);
    const KeyboardMapping& keyboardMapping = Storage::getInstance().getKeyboardMapping();

    writeDoc(doc, "Up", keyboardMapping.keyDpadUp);
    writeDoc(doc, "Down", keyboardMapping.keyDpadDown);
    writeDoc(doc, "Left", keyboardMapping.keyDpadLeft);
    writeDoc(doc, "Right", keyboardMapping.keyDpadRight);
    writeDoc(doc, "B1", keyboardMapping.keyButtonB1);
    writeDoc(doc, "B2", keyboardMapping.keyButtonB2);
    writeDoc(doc, "B3", keyboardMapping.keyButtonB3);
    writeDoc(doc, "B4", keyboardMapping.keyButtonB4);
    writeDoc(doc, "L1", keyboardMapping.keyButtonL1);
    writeDoc(doc, "R1", keyboardMapping.keyButtonR1);
    writeDoc(doc, "L2", keyboardMapping.keyButtonL2);
    writeDoc(doc, "R2", keyboardMapping.keyButtonR2);
    writeDoc(doc, "S1", keyboardMapping.keyButtonS1);
    writeDoc(doc, "S2", keyboardMapping.keyButtonS2);
    writeDoc(doc, "L3", keyboardMapping.keyButtonL3);
    writeDoc(doc, "R3", keyboardMapping.keyButtonR3);
    writeDoc(doc, "A1", keyboardMapping.keyButtonA1);
    writeDoc(doc, "A2", keyboardMapping.keyButtonA2);
    writeDoc(doc, "A3", keyboardMapping.keyButtonA3);
    writeDoc(doc, "A4", keyboardMapping.keyButtonA4);
    writeDoc(doc, "E1", keyboardMapping.keyButtonE1);
    writeDoc(doc, "E2", keyboardMapping.keyButtonE2);
    writeDoc(doc, "E3", keyboardMapping.keyButtonE3);
    writeDoc(doc, "E4", keyboardMapping.keyButtonE4);
    writeDoc(doc, "E5", keyboardMapping.keyButtonE5);
    writeDoc(doc, "E6", keyboardMapping.keyButtonE6);
    writeDoc(doc, "E7", keyboardMapping.keyButtonE7);
    writeDoc(doc, "E8", keyboardMapping.keyButtonE8);
    writeDoc(doc, "E9", keyboardMapping.keyButtonE9);
    writeDoc(doc, "E10", keyboardMapping.keyButtonE10);
    writeDoc(doc, "E11", keyboardMapping.keyButtonE11);
    writeDoc(doc, "E12", keyboardMapping.keyButtonE12);
    
    return serialize_json(doc);
}

std::string getPeripheralOptions()
{
    const size_t capacity = JSON_OBJECT_SIZE(100);
    DynamicJsonDocument doc(capacity);
    const PeripheralOptions& peripheralOptions = Storage::getInstance().getPeripheralOptions();

    writeDoc(doc, "peripheral", "i2c0", "enabled", peripheralOptions.blockI2C0.enabled);
    writeDoc(doc, "peripheral", "i2c0", "sda",     peripheralOptions.blockI2C0.sda);
    writeDoc(doc, "peripheral", "i2c0", "scl",     peripheralOptions.blockI2C0.scl);
    writeDoc(doc, "peripheral", "i2c0", "speed",   peripheralOptions.blockI2C0.speed);

    writeDoc(doc, "peripheral", "i2c1", "enabled", peripheralOptions.blockI2C1.enabled);
    writeDoc(doc, "peripheral", "i2c1", "sda",     peripheralOptions.blockI2C1.sda);
    writeDoc(doc, "peripheral", "i2c1", "scl",     peripheralOptions.blockI2C1.scl);
    writeDoc(doc, "peripheral", "i2c1", "speed",   peripheralOptions.blockI2C1.speed);

    writeDoc(doc, "peripheral", "spi0", "enabled", peripheralOptions.blockSPI0.enabled);
    writeDoc(doc, "peripheral", "spi0", "rx",      peripheralOptions.blockSPI0.rx);
    writeDoc(doc, "peripheral", "spi0", "cs",      peripheralOptions.blockSPI0.cs);
    writeDoc(doc, "peripheral", "spi0", "sck",     peripheralOptions.blockSPI0.sck);
    writeDoc(doc, "peripheral", "spi0", "tx",      peripheralOptions.blockSPI0.tx);

    writeDoc(doc, "peripheral", "spi1", "enabled", peripheralOptions.blockSPI1.enabled);
    writeDoc(doc, "peripheral", "spi1", "rx",      peripheralOptions.blockSPI1.rx);
    writeDoc(doc, "peripheral", "spi1", "cs",      peripheralOptions.blockSPI1.cs);
    writeDoc(doc, "peripheral", "spi1", "sck",     peripheralOptions.blockSPI1.sck);
    writeDoc(doc, "peripheral", "spi1", "tx",      peripheralOptions.blockSPI1.tx);

    writeDoc(doc, "peripheral", "usb0", "enabled", peripheralOptions.blockUSB0.enabled);
    writeDoc(doc, "peripheral", "usb0", "dp",      peripheralOptions.blockUSB0.dp);
    writeDoc(doc, "peripheral", "usb0", "enable5v",peripheralOptions.blockUSB0.enable5v);
    writeDoc(doc, "peripheral", "usb0", "order",   peripheralOptions.blockUSB0.order);

    return serialize_json(doc);
}

std::string getI2CPeripheralMap() {
    const size_t capacity = JSON_OBJECT_SIZE(500);
    DynamicJsonDocument doc(capacity);

    PeripheralOptions& peripheralOptions = Storage::getInstance().getPeripheralOptions();


    if (peripheralOptions.blockI2C0.enabled && PeripheralManager::getInstance().isI2CEnabled(0)) {
        std::map<uint8_t,bool> result = PeripheralManager::getInstance().getI2C(0)->scan();
        for (std::map<uint8_t,bool>::iterator it = result.begin(); it != result.end(); ++it) {
            writeDoc(doc, "i2c0", std::to_string(it->first), it->second);
        }
    }

    if (peripheralOptions.blockI2C1.enabled && PeripheralManager::getInstance().isI2CEnabled(1)) {
        std::map<uint8_t,bool> result = PeripheralManager::getInstance().getI2C(1)->scan();
        for (std::map<uint8_t,bool>::iterator it = result.begin(); it != result.end(); ++it) {
            writeDoc(doc, "i2c1", std::to_string(it->first), it->second);
        }
    }

    return serialize_json(doc);
}

std::string setPeripheralOptions()
{
    DynamicJsonDocument doc = get_post_data();

    PeripheralOptions& peripheralOptions = Storage::getInstance().getPeripheralOptions();

    docToValue(peripheralOptions.blockI2C0.enabled, doc, "peripheral", "i2c0", "enabled");
    docToPin(peripheralOptions.blockI2C0.sda, doc, "peripheral", "i2c0", "sda");
    docToPin(peripheralOptions.blockI2C0.scl, doc, "peripheral", "i2c0", "scl");
    docToValue(peripheralOptions.blockI2C0.speed, doc, "peripheral", "i2c0", "speed");

    docToValue(peripheralOptions.blockI2C1.enabled, doc, "peripheral", "i2c1", "enabled");
    docToPin(peripheralOptions.blockI2C1.sda, doc, "peripheral", "i2c1", "sda");
    docToPin(peripheralOptions.blockI2C1.scl, doc, "peripheral", "i2c1", "scl");
    docToValue(peripheralOptions.blockI2C1.speed, doc, "peripheral", "i2c1", "speed");

    docToValue(peripheralOptions.blockSPI0.enabled, doc,  "peripheral", "spi0", "enabled");
    docToPin(peripheralOptions.blockSPI0.rx, doc,  "peripheral", "spi0", "rx");
    docToPin(peripheralOptions.blockSPI0.cs, doc,  "peripheral", "spi0", "cs");
    docToPin(peripheralOptions.blockSPI0.sck, doc, "peripheral", "spi0", "sck");
    docToPin(peripheralOptions.blockSPI0.tx, doc,  "peripheral", "spi0", "tx");

    docToValue(peripheralOptions.blockSPI1.enabled, doc,  "peripheral", "spi1", "enabled");
    docToPin(peripheralOptions.blockSPI1.rx, doc,  "peripheral", "spi1", "rx");
    docToPin(peripheralOptions.blockSPI1.cs, doc,  "peripheral", "spi1", "cs");
    docToPin(peripheralOptions.blockSPI1.sck, doc, "peripheral", "spi1", "sck");
    docToPin(peripheralOptions.blockSPI1.tx, doc,  "peripheral", "spi1", "tx");

    docToValue(peripheralOptions.blockUSB0.enabled, doc, "peripheral", "usb0", "enabled");
    docToValue(peripheralOptions.blockUSB0.enable5v, doc, "peripheral", "usb0", "enable5v");
    docToValue(peripheralOptions.blockUSB0.order, doc, "peripheral", "usb0", "order");

    // need to reserve previous/next pin for dp
    GpioMappingInfo* gpioMappings = Storage::getInstance().getGpioMappings().pins;
    ProfileOptions& profiles = Storage::getInstance().getProfileOptions();
    uint8_t adjacent = peripheralOptions.blockUSB0.order ? -1 : 1;

    Pin_t oldPinDplus = peripheralOptions.blockUSB0.dp;
    docToPin(peripheralOptions.blockUSB0.dp, doc, "peripheral", "usb0", "dp");
    if (isValidPin(peripheralOptions.blockUSB0.dp)) {
        // if D+ pin is now set, also set the pin that will be used for D-
        gpioMappings[peripheralOptions.blockUSB0.dp+adjacent].action = GpioAction::ASSIGNED_TO_ADDON;
        profiles.gpioMappingsSets[0].pins[peripheralOptions.blockUSB0.dp+adjacent].action =
            GpioAction::ASSIGNED_TO_ADDON;
        profiles.gpioMappingsSets[1].pins[peripheralOptions.blockUSB0.dp+adjacent].action =
            GpioAction::ASSIGNED_TO_ADDON;
        profiles.gpioMappingsSets[2].pins[peripheralOptions.blockUSB0.dp+adjacent].action =
            GpioAction::ASSIGNED_TO_ADDON;
    } else if (isValidPin(oldPinDplus)) {
        // if D+ pin was set and is no longer, also unset the pin that was used for D-
        gpioMappings[oldPinDplus+adjacent].action = GpioAction::NONE;
        profiles.gpioMappingsSets[0].pins[oldPinDplus+adjacent].action = GpioAction::NONE;
        profiles.gpioMappingsSets[1].pins[oldPinDplus+adjacent].action = GpioAction::NONE;
        profiles.gpioMappingsSets[2].pins[oldPinDplus+adjacent].action = GpioAction::NONE;
    }

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return serialize_json(doc);
}

std::string getExpansionPins()
{
    const size_t capacity = JSON_OBJECT_SIZE(100);
    DynamicJsonDocument doc(capacity);
    GpioMappingInfo* gpioMappings = Storage::getInstance().getAddonOptions().pcf8575Options.pins;

    writeDoc(doc, "pins", "pcf8575", 0, "pin00", "option", gpioMappings[0].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin00", "direction", gpioMappings[0].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin01", "option", gpioMappings[1].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin01", "direction", gpioMappings[1].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin02", "option", gpioMappings[2].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin02", "direction", gpioMappings[2].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin03", "option", gpioMappings[3].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin03", "direction", gpioMappings[3].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin04", "option", gpioMappings[4].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin04", "direction", gpioMappings[4].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin05", "option", gpioMappings[5].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin05", "direction", gpioMappings[5].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin06", "option", gpioMappings[6].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin06", "direction", gpioMappings[6].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin07", "option", gpioMappings[7].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin07", "direction", gpioMappings[7].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin08", "option", gpioMappings[8].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin08", "direction", gpioMappings[8].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin09", "option", gpioMappings[9].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin09", "direction", gpioMappings[9].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin10", "option", gpioMappings[10].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin10", "direction", gpioMappings[10].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin11", "option", gpioMappings[11].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin11", "direction", gpioMappings[11].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin12", "option", gpioMappings[12].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin12", "direction", gpioMappings[12].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin13", "option", gpioMappings[13].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin13", "direction", gpioMappings[13].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin14", "option", gpioMappings[14].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin14", "direction", gpioMappings[14].direction);
    writeDoc(doc, "pins", "pcf8575", 0, "pin15", "option", gpioMappings[15].action);
    writeDoc(doc, "pins", "pcf8575", 0, "pin15", "direction", gpioMappings[15].direction);

    return serialize_json(doc);
}

std::string setExpansionPins()
{
    DynamicJsonDocument doc = get_post_data();

    GpioMappingInfo* gpioMappings = Storage::getInstance().getAddonOptions().pcf8575Options.pins;

    char pinName[6];
    for (uint16_t pin = 0; pin < 16; pin++) {
        snprintf(pinName, 6, "pin%0*d", 2, pin);
        // setting a pin shouldn't change a new existing addon/reserved pin
        if (gpioMappings[pin].action != GpioAction::RESERVED &&
                gpioMappings[pin].action != GpioAction::ASSIGNED_TO_ADDON &&
                (GpioAction)doc["pins"]["pcf8575"][0][pinName]["option"] != GpioAction::RESERVED &&
                (GpioAction)doc["pins"]["pcf8575"][0][pinName]["option"] != GpioAction::ASSIGNED_TO_ADDON) {
            gpioMappings[pin].action = (GpioAction)doc["pins"]["pcf8575"][0][pinName]["option"];
            gpioMappings[pin].direction = (GpioDirection)doc["pins"]["pcf8575"][0][pinName]["direction"];
        }
    }
    Storage::getInstance().getAddonOptions().pcf8575Options.pins_count = 16;

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return serialize_json(doc);
}

std::string getReactiveLEDs()
{
    const size_t capacity = JSON_OBJECT_SIZE(100);
    DynamicJsonDocument doc(capacity);
    ReactiveLEDInfo* ledInfo = Storage::getInstance().getAddonOptions().reactiveLEDOptions.leds;

    for (uint16_t led = 0; led < 10; led++) {
        writeDoc(doc, "leds", led, "pin", ledInfo[led].pin);
        writeDoc(doc, "leds", led, "action", ledInfo[led].action);
        writeDoc(doc, "leds", led, "modeDown", ledInfo[led].modeDown);
        writeDoc(doc, "leds", led, "modeUp", ledInfo[led].modeUp);
    }

    return serialize_json(doc);
}

std::string setReactiveLEDs()
{
    DynamicJsonDocument doc = get_post_data();

    ReactiveLEDInfo* ledInfo = Storage::getInstance().getAddonOptions().reactiveLEDOptions.leds;

    for (uint16_t led = 0; led < 10; led++) {
        ledInfo[led].pin = doc["leds"][led]["pin"];
        ledInfo[led].action = doc["leds"][led]["action"];
        ledInfo[led].modeDown = doc["leds"][led]["modeDown"];
        ledInfo[led].modeUp = doc["leds"][led]["modeUp"];
    }
    Storage::getInstance().getAddonOptions().reactiveLEDOptions.leds_count = 10;

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return serialize_json(doc);
}

std::string setAddonOptions()
{
    DynamicJsonDocument doc = get_post_data();

    GpioMappingInfo* gpioMappings = Storage::getInstance().getGpioMappings().pins;

    AnalogOptions& analogOptions = Storage::getInstance().getAddonOptions().analogOptions;
    docToPin(analogOptions.analogAdc1PinX, doc, "analogAdc1PinX");
    docToPin(analogOptions.analogAdc1PinY, doc, "analogAdc1PinY");
    docToValue(analogOptions.analogAdc1Mode, doc, "analogAdc1Mode");
    docToValue(analogOptions.analogAdc1Invert, doc, "analogAdc1Invert");
    docToPin(analogOptions.analogAdc2PinX, doc, "analogAdc2PinX");
    docToPin(analogOptions.analogAdc2PinY, doc, "analogAdc2PinY");
    docToValue(analogOptions.analogAdc2Mode, doc, "analogAdc2Mode");
    docToValue(analogOptions.analogAdc2Invert, doc, "analogAdc2Invert");
    docToValue(analogOptions.forced_circularity, doc, "forced_circularity");
    docToValue(analogOptions.forced_circularity2, doc, "forced_circularity2");
    docToValue(analogOptions.inner_deadzone, doc, "inner_deadzone");
    docToValue(analogOptions.inner_deadzone2, doc, "inner_deadzone2");
    docToValue(analogOptions.outer_deadzone, doc, "outer_deadzone");
    docToValue(analogOptions.outer_deadzone2, doc, "outer_deadzone2");
    docToValue(analogOptions.auto_calibrate, doc, "auto_calibrate");
    docToValue(analogOptions.auto_calibrate2, doc, "auto_calibrate2");
    docToValue(analogOptions.analog_smoothing, doc, "analog_smoothing");
    docToValue(analogOptions.analog_smoothing2, doc, "analog_smoothing2");
    docToValue(analogOptions.smoothing_factor, doc, "smoothing_factor");
    docToValue(analogOptions.smoothing_factor2, doc, "smoothing_factor2");
    docToValue(analogOptions.analog_error, doc, "analog_error");
    docToValue(analogOptions.analog_error2, doc, "analog_error2");
    docToValue(analogOptions.enabled, doc, "AnalogInputEnabled");

    BootselButtonOptions& bootselButtonOptions = Storage::getInstance().getAddonOptions().bootselButtonOptions;
    docToValue(bootselButtonOptions.buttonMap, doc, "bootselButtonMap");
    docToValue(bootselButtonOptions.enabled, doc, "BootselButtonAddonEnabled");

    BuzzerOptions& buzzerOptions = Storage::getInstance().getAddonOptions().buzzerOptions;
    docToPin(buzzerOptions.pin, doc, "buzzerPin");
    docToValue(buzzerOptions.volume, doc, "buzzerVolume");
    docToValue(buzzerOptions.enablePin, doc, "buzzerEnablePin");
    docToValue(buzzerOptions.enabled, doc, "BuzzerSpeakerAddonEnabled");

    DualDirectionalOptions& dualDirectionalOptions = Storage::getInstance().getAddonOptions().dualDirectionalOptions;
    docToValue(dualDirectionalOptions.dpadMode, doc, "dualDirDpadMode");
    docToValue(dualDirectionalOptions.combineMode, doc, "dualDirCombineMode");
    docToValue(dualDirectionalOptions.fourWayMode, doc, "dualDirFourWayMode");
    docToValue(dualDirectionalOptions.enabled, doc, "DualDirectionalInputEnabled");

    TiltOptions& tiltOptions = Storage::getInstance().getAddonOptions().tiltOptions;
    docToValue(tiltOptions.factorTilt1LeftX, doc, "factorTilt1LeftX");
    docToValue(tiltOptions.factorTilt1LeftY, doc, "factorTilt1LeftY");
    docToValue(tiltOptions.factorTilt1RightX, doc, "factorTilt1RightX");
    docToValue(tiltOptions.factorTilt1RightY, doc, "factorTilt1RightY");
    docToValue(tiltOptions.factorTilt2LeftX, doc, "factorTilt2LeftX");
    docToValue(tiltOptions.factorTilt2LeftY, doc, "factorTilt2LeftY");
    docToValue(tiltOptions.factorTilt2RightX, doc, "factorTilt2RightX");
    docToValue(tiltOptions.factorTilt2RightY, doc, "factorTilt2RightY");
    docToValue(tiltOptions.tiltSOCDMode, doc, "tiltSOCDMode");
    docToValue(tiltOptions.enabled, doc, "TiltInputEnabled");

    FocusModeOptions& focusModeOptions = Storage::getInstance().getAddonOptions().focusModeOptions;
    docToValue(focusModeOptions.buttonLockMask, doc, "focusModeButtonLockMask");
    docToValue(focusModeOptions.buttonLockEnabled, doc, "focusModeButtonLockEnabled");
    docToValue(focusModeOptions.macroLockEnabled, doc, "focusModeMacroLockEnabled");
    docToValue(focusModeOptions.enabled, doc, "FocusModeAddonEnabled");

    AnalogADS1219Options& analogADS1219Options = Storage::getInstance().getAddonOptions().analogADS1219Options;
    docToValue(analogADS1219Options.enabled, doc, "I2CAnalog1219InputEnabled");

    ReverseOptions& reverseOptions = Storage::getInstance().getAddonOptions().reverseOptions;
    docToValue(reverseOptions.enabled, doc, "ReverseInputEnabled");
    docToPin(reverseOptions.ledPin, doc, "reversePinLED");
    docToValue(reverseOptions.actionUp, doc, "reverseActionUp");
    docToValue(reverseOptions.actionDown, doc, "reverseActionDown");
    docToValue(reverseOptions.actionLeft, doc, "reverseActionLeft");
    docToValue(reverseOptions.actionRight, doc, "reverseActionRight");

    SOCDSliderOptions& socdSliderOptions = Storage::getInstance().getAddonOptions().socdSliderOptions;
    docToValue(socdSliderOptions.enabled, doc, "SliderSOCDInputEnabled");
    docToValue(socdSliderOptions.modeDefault, doc, "sliderSOCDModeDefault");

    OnBoardLedOptions& onBoardLedOptions = Storage::getInstance().getAddonOptions().onBoardLedOptions;
    docToValue(onBoardLedOptions.mode, doc, "onBoardLedMode");
    docToValue(onBoardLedOptions.enabled, doc, "BoardLedAddonEnabled");

    TurboOptions& turboOptions = Storage::getInstance().getAddonOptions().turboOptions;
    docToPin(turboOptions.ledPin, doc, "turboPinLED");
    docToValue(turboOptions.shotCount, doc, "turboShotCount");
    docToValue(turboOptions.shmupModeEnabled, doc, "shmupMode");
    docToValue(turboOptions.shmupMixMode, doc, "shmupMixMode");
    docToValue(turboOptions.shmupAlwaysOn1, doc, "shmupAlwaysOn1");
    docToValue(turboOptions.shmupAlwaysOn2, doc, "shmupAlwaysOn2");
    docToValue(turboOptions.shmupAlwaysOn3, doc, "shmupAlwaysOn3");
    docToValue(turboOptions.shmupAlwaysOn4, doc, "shmupAlwaysOn4");
    docToPin(turboOptions.shmupBtn1Pin, doc, "pinShmupBtn1");
    docToPin(turboOptions.shmupBtn2Pin, doc, "pinShmupBtn2");
    docToPin(turboOptions.shmupBtn3Pin, doc, "pinShmupBtn3");
    docToPin(turboOptions.shmupBtn4Pin, doc, "pinShmupBtn4");
    docToValue(turboOptions.shmupBtnMask1, doc, "shmupBtnMask1");
    docToValue(turboOptions.shmupBtnMask2, doc, "shmupBtnMask2");
    docToValue(turboOptions.shmupBtnMask3, doc, "shmupBtnMask3");
    docToValue(turboOptions.shmupBtnMask4, doc, "shmupBtnMask4");
    docToPin(turboOptions.shmupDialPin, doc, "pinShmupDial");
    docToValue(turboOptions.turboLedType, doc, "turboLedType");
    docToValue(turboOptions.turboLedIndex, doc, "turboLedIndex");
    docToValue(turboOptions.turboLedColor, doc, "turboLedColor");    
    docToValue(turboOptions.enabled, doc, "TurboInputEnabled");

    WiiOptions& wiiOptions = Storage::getInstance().getAddonOptions().wiiOptions;
    docToValue(wiiOptions.enabled, doc, "WiiExtensionAddonEnabled");

    SNESOptions& snesOptions = Storage::getInstance().getAddonOptions().snesOptions;
    docToValue(snesOptions.enabled, doc, "SNESpadAddonEnabled");
    docToPin(snesOptions.clockPin, doc, "snesPadClockPin");
    docToPin(snesOptions.latchPin, doc, "snesPadLatchPin");
    docToPin(snesOptions.dataPin, doc, "snesPadDataPin");

    KeyboardHostOptions& keyboardHostOptions = Storage::getInstance().getAddonOptions().keyboardHostOptions;
    docToValue(keyboardHostOptions.enabled, doc, "KeyboardHostAddonEnabled");
    docToValue(keyboardHostOptions.mapping.keyDpadUp, doc, "keyboardHostMap", "Up");
    docToValue(keyboardHostOptions.mapping.keyDpadDown, doc, "keyboardHostMap", "Down");
    docToValue(keyboardHostOptions.mapping.keyDpadLeft, doc, "keyboardHostMap", "Left");
    docToValue(keyboardHostOptions.mapping.keyDpadRight, doc, "keyboardHostMap", "Right");
    docToValue(keyboardHostOptions.mapping.keyButtonB1, doc, "keyboardHostMap", "B1");
    docToValue(keyboardHostOptions.mapping.keyButtonB2, doc, "keyboardHostMap", "B2");
    docToValue(keyboardHostOptions.mapping.keyButtonB3, doc, "keyboardHostMap", "B3");
    docToValue(keyboardHostOptions.mapping.keyButtonB4, doc, "keyboardHostMap", "B4");
    docToValue(keyboardHostOptions.mapping.keyButtonL1, doc, "keyboardHostMap", "L1");
    docToValue(keyboardHostOptions.mapping.keyButtonR1, doc, "keyboardHostMap", "R1");
    docToValue(keyboardHostOptions.mapping.keyButtonL2, doc, "keyboardHostMap", "L2");
    docToValue(keyboardHostOptions.mapping.keyButtonR2, doc, "keyboardHostMap", "R2");
    docToValue(keyboardHostOptions.mapping.keyButtonS1, doc, "keyboardHostMap", "S1");
    docToValue(keyboardHostOptions.mapping.keyButtonS2, doc, "keyboardHostMap", "S2");
    docToValue(keyboardHostOptions.mapping.keyButtonL3, doc, "keyboardHostMap", "L3");
    docToValue(keyboardHostOptions.mapping.keyButtonR3, doc, "keyboardHostMap", "R3");
    docToValue(keyboardHostOptions.mapping.keyButtonA1, doc, "keyboardHostMap", "A1");
    docToValue(keyboardHostOptions.mapping.keyButtonA2, doc, "keyboardHostMap", "A2");
    docToValue(keyboardHostOptions.mapping.keyButtonA3, doc, "keyboardHostMap", "A3");
    docToValue(keyboardHostOptions.mapping.keyButtonA4, doc, "keyboardHostMap", "A4");
    docToValue(keyboardHostOptions.mouseLeft, doc, "keyboardHostMouseLeft");
    docToValue(keyboardHostOptions.mouseMiddle, doc, "keyboardHostMouseMiddle");
    docToValue(keyboardHostOptions.mouseRight, doc, "keyboardHostMouseRight");

    GamepadUSBHostOptions& gamepadUSBHostOptions = Storage::getInstance().getAddonOptions().gamepadUSBHostOptions;
    docToValue(gamepadUSBHostOptions.enabled, doc, "GamepadUSBHostAddonEnabled");

    AnalogADS1256Options& ads1256Options = Storage::getInstance().getAddonOptions().analogADS1256Options;
    docToValue(ads1256Options.enabled, doc, "Analog1256Enabled");
    docToValue(ads1256Options.spiBlock, doc, "analog1256Block");
    docToValue(ads1256Options.csPin, doc, "analog1256CsPin");
    docToValue(ads1256Options.drdyPin, doc, "analog1256DrdyPin");
    docToValue(ads1256Options.avdd, doc, "analog1256AnalogMax");
    docToValue(ads1256Options.enableTriggers, doc, "analog1256EnableTriggers");

    RotaryOptions& rotaryOptions = Storage::getInstance().getAddonOptions().rotaryOptions;
    docToValue(rotaryOptions.enabled, doc, "RotaryAddonEnabled");
    docToValue(rotaryOptions.encoderOne.enabled, doc, "encoderOneEnabled");
    docToValue(rotaryOptions.encoderOne.pinA, doc, "encoderOnePinA");
    docToValue(rotaryOptions.encoderOne.pinB, doc, "encoderOnePinB");
    docToValue(rotaryOptions.encoderOne.mode, doc, "encoderOneMode");
    docToValue(rotaryOptions.encoderOne.pulsesPerRevolution, doc, "encoderOnePPR");
    docToValue(rotaryOptions.encoderOne.resetAfter, doc, "encoderOneResetAfter");
    docToValue(rotaryOptions.encoderOne.allowWrapAround, doc, "encoderOneAllowWrapAround");
    docToValue(rotaryOptions.encoderOne.multiplier, doc, "encoderOneMultiplier");
    docToValue(rotaryOptions.encoderTwo.enabled, doc, "encoderTwoEnabled");
    docToValue(rotaryOptions.encoderTwo.pinA, doc, "encoderTwoPinA");
    docToValue(rotaryOptions.encoderTwo.pinB, doc, "encoderTwoPinB");
    docToValue(rotaryOptions.encoderTwo.mode, doc, "encoderTwoMode");
    docToValue(rotaryOptions.encoderTwo.pulsesPerRevolution, doc, "encoderTwoPPR");
    docToValue(rotaryOptions.encoderTwo.resetAfter, doc, "encoderTwoResetAfter");
    docToValue(rotaryOptions.encoderTwo.allowWrapAround, doc, "encoderTwoAllowWrapAround");
    docToValue(rotaryOptions.encoderTwo.multiplier, doc, "encoderTwoMultiplier");

    PCF8575Options& pcf8575Options = Storage::getInstance().getAddonOptions().pcf8575Options;
    docToValue(pcf8575Options.enabled, doc, "PCF8575AddonEnabled");

    ReactiveLEDOptions& reactiveLEDOptions = Storage::getInstance().getAddonOptions().reactiveLEDOptions;
    docToValue(reactiveLEDOptions.enabled, doc, "ReactiveLEDAddonEnabled");

    DRV8833RumbleOptions& drv8833RumbleOptions = Storage::getInstance().getAddonOptions().drv8833RumbleOptions;
    docToValue(drv8833RumbleOptions.enabled, doc, "DRV8833RumbleAddonEnabled");
    docToPin(drv8833RumbleOptions.leftMotorPin, doc, "drv8833RumbleLeftMotorPin");
    docToPin(drv8833RumbleOptions.rightMotorPin, doc, "drv8833RumbleRightMotorPin");
    docToPin(drv8833RumbleOptions.motorSleepPin, doc, "drv8833RumbleMotorSleepPin");
    docToValue(drv8833RumbleOptions.pwmFrequency, doc, "drv8833RumblePWMFrequency");
    docToValue(drv8833RumbleOptions.dutyMin, doc, "drv8833RumbleDutyMin");
    docToValue(drv8833RumbleOptions.dutyMax, doc, "drv8833RumbleDutyMax");

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return serialize_json(doc);
}

std::string setPS4Options()
{
    DynamicJsonDocument doc = get_post_data();
    PS4Options& ps4Options = Storage::getInstance().getAddonOptions().ps4Options;
    std::string encoded;
    std::string decoded;

    const auto readEncoded = [&](const char* key) -> bool
    {
        if (doc.containsKey(key))
        {
            const char* str = nullptr;
            readDoc(str, doc, key);
            encoded = str;
            return true;
        }
        else
        {
            return false;
        }
    };

    // RSA Context
    if ( readEncoded("N") ) {
        if ( Base64::Decode(encoded, decoded) && (decoded.length() == sizeof(ps4Options.rsaN.bytes)) ) {
            memcpy(ps4Options.rsaN.bytes, decoded.data(), decoded.length());
            ps4Options.rsaN.size = decoded.length();
        }
    }
    if ( readEncoded("E") ) {
        if ( Base64::Decode(encoded, decoded) && (decoded.length() == sizeof(ps4Options.rsaE.bytes)) ) {
            memcpy(ps4Options.rsaE.bytes, decoded.data(), decoded.length());
            ps4Options.rsaE.size = decoded.length();
        }
    }
    if ( readEncoded("P") ) {
        if ( Base64::Decode(encoded, decoded) && (decoded.length() == sizeof(ps4Options.rsaP.bytes)) ) {
            memcpy(ps4Options.rsaP.bytes, decoded.data(), decoded.length());
            ps4Options.rsaP.size = decoded.length();
        }
    }
    if ( readEncoded("Q") ) {
        if ( Base64::Decode(encoded, decoded) && (decoded.length() == sizeof(ps4Options.rsaQ.bytes)) ) {
            memcpy(ps4Options.rsaQ.bytes, decoded.data(), decoded.length());
            ps4Options.rsaQ.size = decoded.length();
        }
    }
    // Serial & Signature
    if ( readEncoded("serial") ) {
        if ( Base64::Decode(encoded, decoded) && (decoded.length() == sizeof(ps4Options.serial.bytes)) ) {
            memcpy(ps4Options.serial.bytes, decoded.data(), decoded.length());
            ps4Options.serial.size = decoded.length();
        }
    }
    if ( readEncoded("signature") ) {
        if ( Base64::Decode(encoded, decoded) && (decoded.length() == sizeof(ps4Options.signature.bytes)) ) {
            memcpy(ps4Options.signature.bytes, decoded.data(), decoded.length());
            ps4Options.signature.size = decoded.length();
        }
    }

    // Zap deprecated fields
    if (ps4Options.rsaD.size != 0) ps4Options.rsaD.size = 0;
    if (ps4Options.rsaDP.size != 0) ps4Options.rsaDP.size = 0;
    if (ps4Options.rsaDQ.size != 0) ps4Options.rsaDQ.size = 0;
    if (ps4Options.rsaQP.size != 0) ps4Options.rsaQP.size = 0;
    if (ps4Options.rsaRN.size != 0) ps4Options.rsaRN.size = 0;

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return "{\"success\":true}";
}

std::string setWiiControls()
{
    DynamicJsonDocument doc = get_post_data();
    WiiOptions& wiiOptions = Storage::getInstance().getAddonOptions().wiiOptions;

    readDoc(wiiOptions.controllers.nunchuk.buttonC, doc, "nunchuk.buttonC");
    readDoc(wiiOptions.controllers.nunchuk.buttonZ, doc, "nunchuk.buttonZ");
    readDoc(wiiOptions.controllers.nunchuk.stick.x.axisType, doc, "nunchuk.analogStick.x.axisType");
    readDoc(wiiOptions.controllers.nunchuk.stick.y.axisType, doc, "nunchuk.analogStick.y.axisType");

    readDoc(wiiOptions.controllers.classic.buttonA, doc, "classic.buttonA");
    readDoc(wiiOptions.controllers.classic.buttonB, doc, "classic.buttonB");
    readDoc(wiiOptions.controllers.classic.buttonX, doc, "classic.buttonX");
    readDoc(wiiOptions.controllers.classic.buttonY, doc, "classic.buttonY");
    readDoc(wiiOptions.controllers.classic.buttonL, doc, "classic.buttonL");
    readDoc(wiiOptions.controllers.classic.buttonZL, doc, "classic.buttonZL");
    readDoc(wiiOptions.controllers.classic.buttonR, doc, "classic.buttonR");
    readDoc(wiiOptions.controllers.classic.buttonZR, doc, "classic.buttonZR");
    readDoc(wiiOptions.controllers.classic.buttonMinus, doc, "classic.buttonMinus");
    readDoc(wiiOptions.controllers.classic.buttonPlus, doc, "classic.buttonPlus");
    readDoc(wiiOptions.controllers.classic.buttonHome, doc, "classic.buttonHome");
    readDoc(wiiOptions.controllers.classic.buttonUp, doc, "classic.buttonUp");
    readDoc(wiiOptions.controllers.classic.buttonDown, doc, "classic.buttonDown");
    readDoc(wiiOptions.controllers.classic.buttonLeft, doc, "classic.buttonLeft");
    readDoc(wiiOptions.controllers.classic.buttonRight, doc, "classic.buttonRight");
    readDoc(wiiOptions.controllers.classic.leftStick.x.axisType, doc, "classic.analogLeftStick.x.axisType");
    readDoc(wiiOptions.controllers.classic.leftStick.y.axisType, doc, "classic.analogLeftStick.y.axisType");
    readDoc(wiiOptions.controllers.classic.rightStick.x.axisType, doc, "classic.analogRightStick.x.axisType");
    readDoc(wiiOptions.controllers.classic.rightStick.y.axisType, doc, "classic.analogRightStick.y.axisType");
    readDoc(wiiOptions.controllers.classic.leftTrigger.axisType, doc, "classic.analogLeftTrigger.axisType");
    readDoc(wiiOptions.controllers.classic.rightTrigger.axisType, doc, "classic.analogRightTrigger.axisType");

    readDoc(wiiOptions.controllers.taiko.buttonKatLeft, doc, "taiko.buttonKatLeft");
    readDoc(wiiOptions.controllers.taiko.buttonKatRight, doc, "taiko.buttonKatRight");
    readDoc(wiiOptions.controllers.taiko.buttonDonLeft, doc, "taiko.buttonDonLeft");
    readDoc(wiiOptions.controllers.taiko.buttonDonRight, doc, "taiko.buttonDonRight");

    readDoc(wiiOptions.controllers.guitar.buttonRed, doc, "guitar.buttonRed");
    readDoc(wiiOptions.controllers.guitar.buttonGreen, doc, "guitar.buttonGreen");
    readDoc(wiiOptions.controllers.guitar.buttonYellow, doc, "guitar.buttonYellow");
    readDoc(wiiOptions.controllers.guitar.buttonBlue, doc, "guitar.buttonBlue");
    readDoc(wiiOptions.controllers.guitar.buttonOrange, doc, "guitar.buttonOrange");
    readDoc(wiiOptions.controllers.guitar.buttonPedal, doc, "guitar.buttonPedal");
    readDoc(wiiOptions.controllers.guitar.buttonMinus, doc, "guitar.buttonMinus");
    readDoc(wiiOptions.controllers.guitar.buttonPlus, doc, "guitar.buttonPlus");
    readDoc(wiiOptions.controllers.guitar.strumUp, doc, "guitar.buttonStrumUp");
    readDoc(wiiOptions.controllers.guitar.strumDown, doc, "guitar.buttonStrumDown");
    readDoc(wiiOptions.controllers.guitar.stick.x.axisType, doc, "guitar.analogStick.x.axisType");
    readDoc(wiiOptions.controllers.guitar.stick.y.axisType, doc, "guitar.analogStick.y.axisType");
    readDoc(wiiOptions.controllers.guitar.whammyBar.axisType, doc, "guitar.analogWhammyBar.axisType");

    readDoc(wiiOptions.controllers.drum.buttonRed, doc, "drum.buttonRed");
    readDoc(wiiOptions.controllers.drum.buttonGreen, doc, "drum.buttonGreen");
    readDoc(wiiOptions.controllers.drum.buttonYellow, doc, "drum.buttonYellow");
    readDoc(wiiOptions.controllers.drum.buttonBlue, doc, "drum.buttonBlue");
    readDoc(wiiOptions.controllers.drum.buttonOrange, doc, "drum.buttonOrange");
    readDoc(wiiOptions.controllers.drum.buttonPedal, doc, "drum.buttonPedal");
    readDoc(wiiOptions.controllers.drum.buttonMinus, doc, "drum.buttonMinus");
    readDoc(wiiOptions.controllers.drum.buttonPlus, doc, "drum.buttonPlus");
    readDoc(wiiOptions.controllers.drum.stick.x.axisType, doc, "drum.analogStick.x.axisType");
    readDoc(wiiOptions.controllers.drum.stick.y.axisType, doc, "drum.analogStick.y.axisType");

    readDoc(wiiOptions.controllers.turntable.buttonLeftRed, doc, "turntable.buttonLeftRed");
    readDoc(wiiOptions.controllers.turntable.buttonLeftGreen, doc, "turntable.buttonLeftGreen");
    readDoc(wiiOptions.controllers.turntable.buttonLeftBlue, doc, "turntable.buttonLeftBlue");
    readDoc(wiiOptions.controllers.turntable.buttonRightRed, doc, "turntable.buttonRightRed");
    readDoc(wiiOptions.controllers.turntable.buttonRightGreen, doc, "turntable.buttonRightGreen");
    readDoc(wiiOptions.controllers.turntable.buttonRightBlue, doc, "turntable.buttonRightBlue");
    readDoc(wiiOptions.controllers.turntable.buttonMinus, doc, "turntable.buttonMinus");
    readDoc(wiiOptions.controllers.turntable.buttonPlus, doc, "turntable.buttonPlus");
    readDoc(wiiOptions.controllers.turntable.buttonEuphoria, doc, "turntable.buttonEuphoria");
    readDoc(wiiOptions.controllers.turntable.stick.x.axisType, doc, "turntable.analogStick.x.axisType");
    readDoc(wiiOptions.controllers.turntable.stick.y.axisType, doc, "turntable.analogStick.y.axisType");
    readDoc(wiiOptions.controllers.turntable.leftTurntable.axisType, doc, "turntable.analogLeftTurntable.axisType");
    readDoc(wiiOptions.controllers.turntable.rightTurntable.axisType, doc, "turntable.analogRightTurntable.axisType");
    readDoc(wiiOptions.controllers.turntable.effects.axisType, doc, "turntable.analogEffects.axisType");
    readDoc(wiiOptions.controllers.turntable.fader.axisType, doc, "turntable.analogFader.axisType");

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return "{\"success\":true}";
}

std::string getWiiControls()
{
    const size_t capacity = JSON_OBJECT_SIZE(100);
    DynamicJsonDocument doc(capacity);
    WiiOptions& wiiOptions = Storage::getInstance().getAddonOptions().wiiOptions;

    writeDoc(doc, "nunchuk.buttonC", wiiOptions.controllers.nunchuk.buttonC);
    writeDoc(doc, "nunchuk.buttonZ", wiiOptions.controllers.nunchuk.buttonZ);
    writeDoc(doc, "nunchuk.analogStick.x.axisType", wiiOptions.controllers.nunchuk.stick.x.axisType);
    writeDoc(doc, "nunchuk.analogStick.y.axisType", wiiOptions.controllers.nunchuk.stick.y.axisType);

    writeDoc(doc, "classic.buttonA", wiiOptions.controllers.classic.buttonA);
    writeDoc(doc, "classic.buttonB", wiiOptions.controllers.classic.buttonB);
    writeDoc(doc, "classic.buttonX", wiiOptions.controllers.classic.buttonX);
    writeDoc(doc, "classic.buttonY", wiiOptions.controllers.classic.buttonY);
    writeDoc(doc, "classic.buttonL", wiiOptions.controllers.classic.buttonL);
    writeDoc(doc, "classic.buttonZL", wiiOptions.controllers.classic.buttonZL);
    writeDoc(doc, "classic.buttonR", wiiOptions.controllers.classic.buttonR);
    writeDoc(doc, "classic.buttonZR", wiiOptions.controllers.classic.buttonZR);
    writeDoc(doc, "classic.buttonMinus", wiiOptions.controllers.classic.buttonMinus);
    writeDoc(doc, "classic.buttonPlus", wiiOptions.controllers.classic.buttonPlus);
    writeDoc(doc, "classic.buttonHome", wiiOptions.controllers.classic.buttonHome);
    writeDoc(doc, "classic.buttonUp", wiiOptions.controllers.classic.buttonUp);
    writeDoc(doc, "classic.buttonDown", wiiOptions.controllers.classic.buttonDown);
    writeDoc(doc, "classic.buttonLeft", wiiOptions.controllers.classic.buttonLeft);
    writeDoc(doc, "classic.buttonRight", wiiOptions.controllers.classic.buttonRight);
    writeDoc(doc, "classic.analogLeftStick.x.axisType", wiiOptions.controllers.classic.leftStick.x.axisType);
    writeDoc(doc, "classic.analogLeftStick.y.axisType", wiiOptions.controllers.classic.leftStick.y.axisType);
    writeDoc(doc, "classic.analogRightStick.x.axisType", wiiOptions.controllers.classic.rightStick.x.axisType);
    writeDoc(doc, "classic.analogRightStick.y.axisType", wiiOptions.controllers.classic.rightStick.y.axisType);
    writeDoc(doc, "classic.analogLeftTrigger.axisType", wiiOptions.controllers.classic.leftTrigger.axisType);
    writeDoc(doc, "classic.analogRightTrigger.axisType", wiiOptions.controllers.classic.rightTrigger.axisType);

    writeDoc(doc, "taiko.buttonKatLeft", wiiOptions.controllers.taiko.buttonKatLeft);
    writeDoc(doc, "taiko.buttonKatRight", wiiOptions.controllers.taiko.buttonKatRight);
    writeDoc(doc, "taiko.buttonDonLeft", wiiOptions.controllers.taiko.buttonDonLeft);
    writeDoc(doc, "taiko.buttonDonRight", wiiOptions.controllers.taiko.buttonDonRight);

    writeDoc(doc, "guitar.buttonRed", wiiOptions.controllers.guitar.buttonRed);
    writeDoc(doc, "guitar.buttonGreen", wiiOptions.controllers.guitar.buttonGreen);
    writeDoc(doc, "guitar.buttonYellow", wiiOptions.controllers.guitar.buttonYellow);
    writeDoc(doc, "guitar.buttonBlue", wiiOptions.controllers.guitar.buttonBlue);
    writeDoc(doc, "guitar.buttonOrange", wiiOptions.controllers.guitar.buttonOrange);
    writeDoc(doc, "guitar.buttonPedal", wiiOptions.controllers.guitar.buttonPedal);
    writeDoc(doc, "guitar.buttonMinus", wiiOptions.controllers.guitar.buttonMinus);
    writeDoc(doc, "guitar.buttonPlus", wiiOptions.controllers.guitar.buttonPlus);
    writeDoc(doc, "guitar.buttonStrumUp", wiiOptions.controllers.guitar.strumUp);
    writeDoc(doc, "guitar.buttonStrumDown", wiiOptions.controllers.guitar.strumDown);
    writeDoc(doc, "guitar.analogStick.x.axisType", wiiOptions.controllers.guitar.stick.x.axisType);
    writeDoc(doc, "guitar.analogStick.y.axisType", wiiOptions.controllers.guitar.stick.y.axisType);
    writeDoc(doc, "guitar.analogWhammyBar.axisType", wiiOptions.controllers.guitar.whammyBar.axisType);

    writeDoc(doc, "drum.buttonRed", wiiOptions.controllers.drum.buttonRed);
    writeDoc(doc, "drum.buttonGreen", wiiOptions.controllers.drum.buttonGreen);
    writeDoc(doc, "drum.buttonYellow", wiiOptions.controllers.drum.buttonYellow);
    writeDoc(doc, "drum.buttonBlue", wiiOptions.controllers.drum.buttonBlue);
    writeDoc(doc, "drum.buttonOrange", wiiOptions.controllers.drum.buttonOrange);
    writeDoc(doc, "drum.buttonPedal", wiiOptions.controllers.drum.buttonPedal);
    writeDoc(doc, "drum.buttonMinus", wiiOptions.controllers.drum.buttonMinus);
    writeDoc(doc, "drum.buttonPlus", wiiOptions.controllers.drum.buttonPlus);
    writeDoc(doc, "drum.analogStick.x.axisType", wiiOptions.controllers.drum.stick.x.axisType);
    writeDoc(doc, "drum.analogStick.y.axisType", wiiOptions.controllers.drum.stick.y.axisType);

    writeDoc(doc, "turntable.buttonLeftRed", wiiOptions.controllers.turntable.buttonLeftRed);
    writeDoc(doc, "turntable.buttonLeftGreen", wiiOptions.controllers.turntable.buttonLeftGreen);
    writeDoc(doc, "turntable.buttonLeftBlue", wiiOptions.controllers.turntable.buttonLeftBlue);
    writeDoc(doc, "turntable.buttonRightRed", wiiOptions.controllers.turntable.buttonRightRed);
    writeDoc(doc, "turntable.buttonRightGreen", wiiOptions.controllers.turntable.buttonRightGreen);
    writeDoc(doc, "turntable.buttonRightBlue", wiiOptions.controllers.turntable.buttonRightBlue);
    writeDoc(doc, "turntable.buttonMinus", wiiOptions.controllers.turntable.buttonMinus);
    writeDoc(doc, "turntable.buttonPlus", wiiOptions.controllers.turntable.buttonPlus);
    writeDoc(doc, "turntable.buttonEuphoria", wiiOptions.controllers.turntable.buttonEuphoria);
    writeDoc(doc, "turntable.analogStick.x.axisType", wiiOptions.controllers.turntable.stick.x.axisType);
    writeDoc(doc, "turntable.analogStick.x.axisType", wiiOptions.controllers.turntable.stick.y.axisType);
    writeDoc(doc, "turntable.analogLeftTurntable.axisType", wiiOptions.controllers.turntable.leftTurntable.axisType);
    writeDoc(doc, "turntable.analogRightTurntable.axisType", wiiOptions.controllers.turntable.rightTurntable.axisType);
    writeDoc(doc, "turntable.analogEffects.axisType", wiiOptions.controllers.turntable.effects.axisType);
    writeDoc(doc, "turntable.analogFader.axisType", wiiOptions.controllers.turntable.fader.axisType);

    return serialize_json(doc);
}

std::string getAddonOptions()
{
    const size_t capacity = JSON_OBJECT_SIZE(500);
    DynamicJsonDocument doc(capacity);

    const AnalogOptions& analogOptions = Storage::getInstance().getAddonOptions().analogOptions;
    writeDoc(doc, "analogAdc1PinX", cleanPin(analogOptions.analogAdc1PinX));
    writeDoc(doc, "analogAdc1PinY", cleanPin(analogOptions.analogAdc1PinY));
    writeDoc(doc, "analogAdc1Mode", analogOptions.analogAdc1Mode);
    writeDoc(doc, "analogAdc1Invert", analogOptions.analogAdc1Invert);
    writeDoc(doc, "analogAdc2PinX", cleanPin(analogOptions.analogAdc2PinX));
    writeDoc(doc, "analogAdc2PinY", cleanPin(analogOptions.analogAdc2PinY));
    writeDoc(doc, "analogAdc2Mode", analogOptions.analogAdc2Mode);
    writeDoc(doc, "analogAdc2Invert", analogOptions.analogAdc2Invert);
    writeDoc(doc, "forced_circularity", analogOptions.forced_circularity);
    writeDoc(doc, "forced_circularity2", analogOptions.forced_circularity2);
    writeDoc(doc, "inner_deadzone", analogOptions.inner_deadzone);
    writeDoc(doc, "inner_deadzone2", analogOptions.inner_deadzone2);
    writeDoc(doc, "outer_deadzone", analogOptions.outer_deadzone);
    writeDoc(doc, "outer_deadzone2", analogOptions.outer_deadzone2);
    writeDoc(doc, "auto_calibrate", analogOptions.auto_calibrate);
    writeDoc(doc, "auto_calibrate2", analogOptions.auto_calibrate2);
    writeDoc(doc, "analog_smoothing", analogOptions.analog_smoothing);
    writeDoc(doc, "analog_smoothing2", analogOptions.analog_smoothing2);
    writeDoc(doc, "smoothing_factor", analogOptions.smoothing_factor);
    writeDoc(doc, "smoothing_factor2", analogOptions.smoothing_factor2);
    writeDoc(doc, "analog_error", analogOptions.analog_error);
    writeDoc(doc, "analog_error2", analogOptions.analog_error2);
    writeDoc(doc, "AnalogInputEnabled", analogOptions.enabled);

    const BootselButtonOptions& bootselButtonOptions = Storage::getInstance().getAddonOptions().bootselButtonOptions;
    writeDoc(doc, "bootselButtonMap", bootselButtonOptions.buttonMap);
    writeDoc(doc, "BootselButtonAddonEnabled", bootselButtonOptions.enabled);

    const BuzzerOptions& buzzerOptions = Storage::getInstance().getAddonOptions().buzzerOptions;
    writeDoc(doc, "buzzerPin", cleanPin(buzzerOptions.pin));
    writeDoc(doc, "buzzerVolume", buzzerOptions.volume);
    writeDoc(doc, "buzzerEnablePin", buzzerOptions.enablePin);
    writeDoc(doc, "BuzzerSpeakerAddonEnabled", buzzerOptions.enabled);

    const DualDirectionalOptions& dualDirectionalOptions = Storage::getInstance().getAddonOptions().dualDirectionalOptions;
    writeDoc(doc, "dualDirDpadMode", dualDirectionalOptions.dpadMode);
    writeDoc(doc, "dualDirCombineMode", dualDirectionalOptions.combineMode);
    writeDoc(doc, "dualDirFourWayMode", dualDirectionalOptions.fourWayMode);
    writeDoc(doc, "DualDirectionalInputEnabled", dualDirectionalOptions.enabled);

    const TiltOptions& tiltOptions = Storage::getInstance().getAddonOptions().tiltOptions;
    writeDoc(doc, "factorTilt1LeftX", tiltOptions.factorTilt1LeftX);
    writeDoc(doc, "factorTilt1LeftY", tiltOptions.factorTilt1LeftY);
    writeDoc(doc, "factorTilt1RightX", tiltOptions.factorTilt1RightX);
    writeDoc(doc, "factorTilt1RightY", tiltOptions.factorTilt1RightY);
    writeDoc(doc, "factorTilt2LeftX", tiltOptions.factorTilt2LeftX);
    writeDoc(doc, "factorTilt2LeftY", tiltOptions.factorTilt2LeftY);
    writeDoc(doc, "factorTilt2RightX", tiltOptions.factorTilt2RightX);
    writeDoc(doc, "factorTilt2RightY", tiltOptions.factorTilt2RightY);
    writeDoc(doc, "tiltSOCDMode", tiltOptions.tiltSOCDMode);
    writeDoc(doc, "TiltInputEnabled", tiltOptions.enabled);

    const AnalogADS1219Options& analogADS1219Options = Storage::getInstance().getAddonOptions().analogADS1219Options;
    writeDoc(doc, "I2CAnalog1219InputEnabled", analogADS1219Options.enabled);

    const ReverseOptions& reverseOptions = Storage::getInstance().getAddonOptions().reverseOptions;
    writeDoc(doc, "reversePinLED", cleanPin(reverseOptions.ledPin));
    writeDoc(doc, "reverseActionUp", reverseOptions.actionUp);
    writeDoc(doc, "reverseActionDown", reverseOptions.actionDown);
    writeDoc(doc, "reverseActionLeft", reverseOptions.actionLeft);
    writeDoc(doc, "reverseActionRight", reverseOptions.actionRight);
    writeDoc(doc, "ReverseInputEnabled", reverseOptions.enabled);

    const SOCDSliderOptions& socdSliderOptions = Storage::getInstance().getAddonOptions().socdSliderOptions;
    writeDoc(doc, "sliderSOCDModeDefault", socdSliderOptions.modeDefault);
    writeDoc(doc, "SliderSOCDInputEnabled", socdSliderOptions.enabled);

    const OnBoardLedOptions& onBoardLedOptions = Storage::getInstance().getAddonOptions().onBoardLedOptions;
    writeDoc(doc, "onBoardLedMode", onBoardLedOptions.mode);
    writeDoc(doc, "BoardLedAddonEnabled", onBoardLedOptions.enabled);

    const TurboOptions& turboOptions = Storage::getInstance().getAddonOptions().turboOptions;
    writeDoc(doc, "turboPinLED", cleanPin(turboOptions.ledPin));
    writeDoc(doc, "turboShotCount", turboOptions.shotCount);
    writeDoc(doc, "shmupMode", turboOptions.shmupModeEnabled);
    writeDoc(doc, "shmupMixMode", turboOptions.shmupMixMode);
    writeDoc(doc, "shmupAlwaysOn1", turboOptions.shmupAlwaysOn1);
    writeDoc(doc, "shmupAlwaysOn2", turboOptions.shmupAlwaysOn2);
    writeDoc(doc, "shmupAlwaysOn3", turboOptions.shmupAlwaysOn3);
    writeDoc(doc, "shmupAlwaysOn4", turboOptions.shmupAlwaysOn4);
    writeDoc(doc, "pinShmupBtn1", cleanPin(turboOptions.shmupBtn1Pin));
    writeDoc(doc, "pinShmupBtn2", cleanPin(turboOptions.shmupBtn2Pin));
    writeDoc(doc, "pinShmupBtn3", cleanPin(turboOptions.shmupBtn3Pin));
    writeDoc(doc, "pinShmupBtn4", cleanPin(turboOptions.shmupBtn4Pin));
    writeDoc(doc, "shmupBtnMask1", turboOptions.shmupBtnMask1);
    writeDoc(doc, "shmupBtnMask2", turboOptions.shmupBtnMask2);
    writeDoc(doc, "shmupBtnMask3", turboOptions.shmupBtnMask3);
    writeDoc(doc, "shmupBtnMask4", turboOptions.shmupBtnMask4);
    writeDoc(doc, "pinShmupDial", cleanPin(turboOptions.shmupDialPin));
    writeDoc(doc, "turboLedType", turboOptions.turboLedType);
    writeDoc(doc, "turboLedIndex", turboOptions.turboLedIndex);
    writeDoc(doc, "turboLedColor",  ((RGB)turboOptions.turboLedColor).value(LED_FORMAT_RGB));
    writeDoc(doc, "TurboInputEnabled", turboOptions.enabled);

    const WiiOptions& wiiOptions = Storage::getInstance().getAddonOptions().wiiOptions;
    writeDoc(doc, "WiiExtensionAddonEnabled", wiiOptions.enabled);

    const SNESOptions& snesOptions = Storage::getInstance().getAddonOptions().snesOptions;
    writeDoc(doc, "snesPadClockPin", cleanPin(snesOptions.clockPin));
    writeDoc(doc, "snesPadLatchPin", cleanPin(snesOptions.latchPin));
    writeDoc(doc, "snesPadDataPin", cleanPin(snesOptions.dataPin));
    writeDoc(doc, "SNESpadAddonEnabled", snesOptions.enabled);

    const KeyboardHostOptions& keyboardHostOptions = Storage::getInstance().getAddonOptions().keyboardHostOptions;
    writeDoc(doc, "KeyboardHostAddonEnabled", keyboardHostOptions.enabled);
    writeDoc(doc, "keyboardHostMap", "Up", keyboardHostOptions.mapping.keyDpadUp);
    writeDoc(doc, "keyboardHostMap", "Down", keyboardHostOptions.mapping.keyDpadDown);
    writeDoc(doc, "keyboardHostMap", "Left", keyboardHostOptions.mapping.keyDpadLeft);
    writeDoc(doc, "keyboardHostMap", "Right", keyboardHostOptions.mapping.keyDpadRight);
    writeDoc(doc, "keyboardHostMap", "B1", keyboardHostOptions.mapping.keyButtonB1);
    writeDoc(doc, "keyboardHostMap", "B2", keyboardHostOptions.mapping.keyButtonB2);
    writeDoc(doc, "keyboardHostMap", "B3", keyboardHostOptions.mapping.keyButtonB3);
    writeDoc(doc, "keyboardHostMap", "B4", keyboardHostOptions.mapping.keyButtonB4);
    writeDoc(doc, "keyboardHostMap", "L1", keyboardHostOptions.mapping.keyButtonL1);
    writeDoc(doc, "keyboardHostMap", "R1", keyboardHostOptions.mapping.keyButtonR1);
    writeDoc(doc, "keyboardHostMap", "L2", keyboardHostOptions.mapping.keyButtonL2);
    writeDoc(doc, "keyboardHostMap", "R2", keyboardHostOptions.mapping.keyButtonR2);
    writeDoc(doc, "keyboardHostMap", "S1", keyboardHostOptions.mapping.keyButtonS1);
    writeDoc(doc, "keyboardHostMap", "S2", keyboardHostOptions.mapping.keyButtonS2);
    writeDoc(doc, "keyboardHostMap", "L3", keyboardHostOptions.mapping.keyButtonL3);
    writeDoc(doc, "keyboardHostMap", "R3", keyboardHostOptions.mapping.keyButtonR3);
    writeDoc(doc, "keyboardHostMap", "A1", keyboardHostOptions.mapping.keyButtonA1);
    writeDoc(doc, "keyboardHostMap", "A2", keyboardHostOptions.mapping.keyButtonA2);
    writeDoc(doc, "keyboardHostMap", "A3", keyboardHostOptions.mapping.keyButtonA3);
    writeDoc(doc, "keyboardHostMap", "A4", keyboardHostOptions.mapping.keyButtonA4);
    writeDoc(doc, "keyboardHostMouseLeft", keyboardHostOptions.mouseLeft);
    writeDoc(doc, "keyboardHostMouseMiddle", keyboardHostOptions.mouseMiddle);
    writeDoc(doc, "keyboardHostMouseRight", keyboardHostOptions.mouseRight);

    const GamepadUSBHostOptions& gamepadUSBHostOptions = Storage::getInstance().getAddonOptions().gamepadUSBHostOptions;
    writeDoc(doc, "GamepadUSBHostAddonEnabled", gamepadUSBHostOptions.enabled);

    AnalogADS1256Options& ads1256Options = Storage::getInstance().getAddonOptions().analogADS1256Options;
    writeDoc(doc, "Analog1256Enabled", ads1256Options.enabled);
    writeDoc(doc, "analog1256Block", ads1256Options.spiBlock);
    writeDoc(doc, "analog1256CsPin", ads1256Options.csPin);
    writeDoc(doc, "analog1256DrdyPin", ads1256Options.drdyPin);
    writeDoc(doc, "analog1256AnalogMax", ads1256Options.avdd);
    writeDoc(doc, "analog1256EnableTriggers", ads1256Options.enableTriggers);

    const FocusModeOptions& focusModeOptions = Storage::getInstance().getAddonOptions().focusModeOptions;
    writeDoc(doc, "focusModeButtonLockMask", focusModeOptions.buttonLockMask);
    writeDoc(doc, "focusModeButtonLockEnabled", focusModeOptions.buttonLockEnabled);
    writeDoc(doc, "focusModeMacroLockEnabled", focusModeOptions.macroLockEnabled);
    writeDoc(doc, "FocusModeAddonEnabled", focusModeOptions.enabled);

    RotaryOptions& rotaryOptions = Storage::getInstance().getAddonOptions().rotaryOptions;
    writeDoc(doc, "RotaryAddonEnabled", rotaryOptions.enabled);
    writeDoc(doc, "encoderOneEnabled", rotaryOptions.encoderOne.enabled);
    writeDoc(doc, "encoderOnePinA", rotaryOptions.encoderOne.pinA);
    writeDoc(doc, "encoderOnePinB", rotaryOptions.encoderOne.pinB);
    writeDoc(doc, "encoderOneMode", rotaryOptions.encoderOne.mode);
    writeDoc(doc, "encoderOnePPR", rotaryOptions.encoderOne.pulsesPerRevolution);
    writeDoc(doc, "encoderOneResetAfter", rotaryOptions.encoderOne.resetAfter);
    writeDoc(doc, "encoderOneAllowWrapAround", rotaryOptions.encoderOne.allowWrapAround);
    writeDoc(doc, "encoderOneMultiplier", rotaryOptions.encoderOne.multiplier);
    writeDoc(doc, "encoderTwoEnabled", rotaryOptions.encoderTwo.enabled);
    writeDoc(doc, "encoderTwoPinA", rotaryOptions.encoderTwo.pinA);
    writeDoc(doc, "encoderTwoPinB", rotaryOptions.encoderTwo.pinB);
    writeDoc(doc, "encoderTwoMode", rotaryOptions.encoderTwo.mode);
    writeDoc(doc, "encoderTwoPPR", rotaryOptions.encoderTwo.pulsesPerRevolution);
    writeDoc(doc, "encoderTwoResetAfter", rotaryOptions.encoderTwo.resetAfter);
    writeDoc(doc, "encoderTwoAllowWrapAround", rotaryOptions.encoderTwo.allowWrapAround);
    writeDoc(doc, "encoderTwoMultiplier", rotaryOptions.encoderTwo.multiplier);

    PCF8575Options& pcf8575Options = Storage::getInstance().getAddonOptions().pcf8575Options;
    writeDoc(doc, "PCF8575AddonEnabled", pcf8575Options.enabled);

    ReactiveLEDOptions& reactiveLEDOptions = Storage::getInstance().getAddonOptions().reactiveLEDOptions;
    writeDoc(doc, "ReactiveLEDAddonEnabled", reactiveLEDOptions.enabled);

    const DRV8833RumbleOptions& drv8833RumbleOptions = Storage::getInstance().getAddonOptions().drv8833RumbleOptions;
    writeDoc(doc, "DRV8833RumbleAddonEnabled", drv8833RumbleOptions.enabled);
    writeDoc(doc, "drv8833RumbleLeftMotorPin", cleanPin(drv8833RumbleOptions.leftMotorPin));
    writeDoc(doc, "drv8833RumbleRightMotorPin", cleanPin(drv8833RumbleOptions.rightMotorPin));
    writeDoc(doc, "drv8833RumbleMotorSleepPin", cleanPin(drv8833RumbleOptions.motorSleepPin));
    writeDoc(doc, "drv8833RumblePWMFrequency", drv8833RumbleOptions.pwmFrequency);
    writeDoc(doc, "drv8833RumbleDutyMin", drv8833RumbleOptions.dutyMin);
    writeDoc(doc, "drv8833RumbleDutyMax", drv8833RumbleOptions.dutyMax);

    return serialize_json(doc);
}

std::string setMacroAddonOptions()
{
    DynamicJsonDocument doc = get_post_data();

    MacroOptions& macroOptions = Storage::getInstance().getAddonOptions().macroOptions;
    docToValue(macroOptions.macroBoardLedEnabled, doc, "macroBoardLedEnabled");

    JsonObject options = doc.as<JsonObject>();
    JsonArray macros = options["macroList"];
    int macrosIndex = 0;

    for (JsonObject macro : macros) {
        size_t macroLabelSize = sizeof(macroOptions.macroList[macrosIndex].macroLabel);
        strncpy(macroOptions.macroList[macrosIndex].macroLabel, macro["macroLabel"], macroLabelSize - 1);
        macroOptions.macroList[macrosIndex].macroLabel[macroLabelSize - 1] = '\0';
        macroOptions.macroList[macrosIndex].macroType = macro["macroType"].as<MacroType>();
        macroOptions.macroList[macrosIndex].useMacroTriggerButton = macro["useMacroTriggerButton"].as<bool>();
        macroOptions.macroList[macrosIndex].macroTriggerButton = macro["macroTriggerButton"].as<uint32_t>();
        macroOptions.macroList[macrosIndex].enabled = macro["enabled"] == true;
        macroOptions.macroList[macrosIndex].exclusive = macro["exclusive"] == true;
        macroOptions.macroList[macrosIndex].interruptible = macro["interruptible"] == true;
        macroOptions.macroList[macrosIndex].showFrames = macro["showFrames"] == true;
        JsonArray macroInputs = macro["macroInputs"];
        int macroInputsIndex = 0;

        for (JsonObject input: macroInputs) {
            macroOptions.macroList[macrosIndex].macroInputs[macroInputsIndex].duration = input["duration"].as<uint32_t>();
            macroOptions.macroList[macrosIndex].macroInputs[macroInputsIndex].waitDuration = input["waitDuration"].as<uint32_t>();
            macroOptions.macroList[macrosIndex].macroInputs[macroInputsIndex].buttonMask = input["buttonMask"].as<uint32_t>();
            if (++macroInputsIndex >= MAX_MACRO_INPUT_LIMIT) break;
        }
        macroOptions.macroList[macrosIndex].macroInputs_count = macroInputsIndex;

        if (++macrosIndex >= MAX_MACRO_LIMIT)
            break;
    }

    macroOptions.macroList_count = MAX_MACRO_LIMIT;

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));
    return serialize_json(doc);
}

std::string getMacroAddonOptions()
{
    const size_t capacity = JSON_OBJECT_SIZE(500);
    DynamicJsonDocument doc(capacity);

    MacroOptions& macroOptions = Storage::getInstance().getAddonOptions().macroOptions;
    JsonArray macroList = doc.createNestedArray("macroList");

    writeDoc(doc, "macroBoardLedEnabled", macroOptions.macroBoardLedEnabled);

    for (int i = 0; i < MAX_MACRO_LIMIT; i++) {
        JsonObject macro = macroList.createNestedObject();
        macro["enabled"] = macroOptions.macroList[i].enabled ? 1 : 0;
        macro["exclusive"] = macroOptions.macroList[i].exclusive ? 1 : 0;
        macro["interruptible"] = macroOptions.macroList[i].interruptible ? 1 : 0;
        macro["showFrames"] = macroOptions.macroList[i].showFrames ? 1 : 0;
        macro["macroType"] = macroOptions.macroList[i].macroType;
        macro["useMacroTriggerButton"] = macroOptions.macroList[i].useMacroTriggerButton ? 1 : 0;
        macro["macroTriggerButton"] = macroOptions.macroList[i].macroTriggerButton;
        macro["macroLabel"] = macroOptions.macroList[i].macroLabel;

        JsonArray macroInputs = macro.createNestedArray("macroInputs");
        for (int j = 0; j < macroOptions.macroList[i].macroInputs_count; j++) {
            JsonObject macroInput = macroInputs.createNestedObject();
            macroInput["buttonMask"] = macroOptions.macroList[i].macroInputs[j].buttonMask;
            macroInput["duration"] = macroOptions.macroList[i].macroInputs[j].duration;
            macroInput["waitDuration"] = macroOptions.macroList[i].macroInputs[j].waitDuration;
        }
    }

    return serialize_json(doc);
}

std::string getFirmwareVersion()
{
    const size_t capacity = JSON_OBJECT_SIZE(10);
    DynamicJsonDocument doc(capacity);
    writeDoc(doc, "version", GP2040VERSION);
    writeDoc(doc, "boardArchitecture", GP2040PLATFORM);
    writeDoc(doc, "boardBuild", GP2040BUILD);
    writeDoc(doc, "boardBuildType", GP2040CONFIG);
    writeDoc(doc, "boardConfigLabel", BOARD_CONFIG_LABEL);
    writeDoc(doc, "boardConfigFileName", BOARD_CONFIG_FILE_NAME);
    writeDoc(doc, "boardConfig", GP2040_BOARDCONFIG);
    return serialize_json(doc);
}

std::string getMemoryReport()
{
    const size_t capacity = JSON_OBJECT_SIZE(10);
    DynamicJsonDocument doc(capacity);
    writeDoc(doc, "totalFlash", System::getTotalFlash());
    writeDoc(doc, "usedFlash", System::getUsedFlash());
    writeDoc(doc, "physicalFlash", Storage::getInstance().GetFlashSize());
    writeDoc(doc, "staticAllocs", System::getStaticAllocs());
    writeDoc(doc, "totalHeap", System::getTotalHeap());
    writeDoc(doc, "usedHeap", System::getUsedHeap());
    return serialize_json(doc);
}

static bool _abortGetHeldPins = false;

std::string getHeldPins()
{
    const size_t capacity = JSON_OBJECT_SIZE(100);
    DynamicJsonDocument doc(capacity);

    // Initialize unassigned pins so that they can be read from
    std::vector<uint> uninitPins;
    for (uint32_t pin = 0; pin < NUM_BANK0_GPIOS; pin++) {
        switch (pin) {
            case 23:
            case 24:
            case 25:
            case 29:
                continue;
        }
        if (gpio_get_function(pin) == GPIO_FUNC_NULL) {
            uninitPins.push_back(pin);
            gpio_init(pin);             // Initialize pin
            gpio_set_dir(pin, GPIO_IN); // Set as INPUT
            gpio_pull_up(pin);          // Set as PULLUP
        }
    }

    uint32_t timePinWait = getMillis();
    uint32_t oldState = ~gpio_get_all();
    uint32_t newState = 0;
    uint32_t debounceStartTime = 0;
    std::set<uint> heldPinsSet;
    bool isAnyPinHeld = false;

    uint32_t currentMillis = 0;
    while ((isAnyPinHeld || (((currentMillis = getMillis()) - timePinWait) < 5000))) { // 5 seconds of idle time
        // rndis http server requires inline functions (non-class)
        rndis_task();

        if (_abortGetHeldPins)
            break;
        if (isAnyPinHeld && newState == oldState) // Should match old state when pins are released
            break;

        newState = ~gpio_get_all();
        uint32_t newPin = newState ^ oldState;
        for (uint32_t pin = 0; pin < NUM_BANK0_GPIOS; pin++) {
            if (gpio_get_function(pin) == GPIO_FUNC_SIO &&
               !gpio_is_dir_out(pin) && (newPin & (1 << pin))) {
                if (debounceStartTime == 0) debounceStartTime = currentMillis;
                if ((currentMillis - debounceStartTime) > 5) { // wait 5ms
                    heldPinsSet.insert(pin);
                    isAnyPinHeld = true;
                }
            }
        }
    }

    auto heldPins = doc.createNestedArray("heldPins");
    for (uint32_t pin : heldPinsSet) {
        heldPins.add(pin);
    }
    for (uint32_t pin: uninitPins) {
        gpio_deinit(pin);
    }

    if (_abortGetHeldPins) {
        _abortGetHeldPins = false;
        return {};
    } else {
        return serialize_json(doc);
    }
}

std::string abortGetHeldPins()
{
    _abortGetHeldPins = true;
    return {};
}

std::string getConfig()
{
    return ConfigUtils::toJSON(Storage::getInstance().getConfig());
}

DataAndStatusCode setConfig()
{
    // Store config struct on the heap to avoid stack overflow
    std::unique_ptr<Config> config(new Config);
    *config.get() = Config Config_init_default;
    if (ConfigUtils::fromJSON(*config.get(), http_post_payload, http_post_payload_len))
    {
        Storage::getInstance().getConfig() = *config.get();
        config.reset();
        if (Storage::getInstance().save(true))
        {
            return DataAndStatusCode(getConfig(), HttpStatusCode::_200);
        }
        else
        {
            return DataAndStatusCode("{ \"error\": \"internal error while saving config\" }", HttpStatusCode::_500);
        }
    }
    else
    {
        return DataAndStatusCode("{ \"error\": \"invalid JSON document\" }", HttpStatusCode::_400);
    }
}

// This should be a storage feature
std::string resetSettings()
{
    Storage::getInstance().ResetSettings();
    const size_t capacity = JSON_OBJECT_SIZE(10);
    DynamicJsonDocument doc(capacity);
    doc["success"] = true;
    return serialize_json(doc);
}

#if !defined(NDEBUG)
std::string echo()
{
    DynamicJsonDocument doc = get_post_data();
    return serialize_json(doc);
}
#endif

// MUST MATCH NAVIGATION.JSX
enum BOOT_MODES {
	GAMEPAD = 0,
	WEBCONFIG = 1,
	BOOTSEL = 2,
};

std::string reboot() {
    DynamicJsonDocument doc = get_post_data();
    uint32_t bootMode = doc["bootMode"];
    System::BootMode systemBootMode = System::BootMode::DEFAULT;
    if ( bootMode == BOOT_MODES::GAMEPAD ) {
        systemBootMode = System::BootMode::GAMEPAD;
    } else if ( bootMode == BOOT_MODES::WEBCONFIG ) {
        systemBootMode = System::BootMode::WEBCONFIG;
    } else if (bootMode == BOOT_MODES::BOOTSEL ) {
        systemBootMode = System::BootMode::USB;
    }
    EventManager::getInstance().triggerEvent(new GPRestartEvent((System::BootMode)systemBootMode));
    doc["success"] = true;
    return serialize_json(doc);
}

typedef std::string (*HandlerFuncPtr)();
static const std::pair<const char*, HandlerFuncPtr> handlerFuncs[] =
{
    { "/api/setDisplayOptions", setDisplayOptions },
    { "/api/setPreviewDisplayOptions", setPreviewDisplayOptions },
    { "/api/setGamepadOptions", setGamepadOptions },
    { "/api/setLedOptions", setLedOptions },
    { "/api/setAnimationButtonTestMode", setAnimationButtonTestMode },
    { "/api/setAnimationButtonTestState", setAnimationButtonTestState },
    { "/api/setAnimationProtoOptions", setAnimationProtoOptions },
    { "/api/getAnimationProtoOptions", getAnimationProtoOptions },
    { "/api/setLightsDataOptions", setLightsDataOptions },
    { "/api/getLightsDataOptions", getLightsDataOptions },
    { "/api/setPinMappings", setPinMappings },
    { "/api/setProfileOptions", setProfileOptions },
    { "/api/setPeripheralOptions", setPeripheralOptions },
    { "/api/getPeripheralOptions", getPeripheralOptions },
    { "/api/getI2CPeripheralMap", getI2CPeripheralMap },
    { "/api/setExpansionPins", setExpansionPins },
    { "/api/getExpansionPins", getExpansionPins },
    { "/api/setReactiveLEDs", setReactiveLEDs },
    { "/api/getReactiveLEDs", getReactiveLEDs },
    { "/api/setKeyMappings", setKeyMappings },
    { "/api/setAddonsOptions", setAddonOptions },
    { "/api/setMacroAddonOptions", setMacroAddonOptions },
    { "/api/setPS4Options", setPS4Options },
    { "/api/setWiiControls", setWiiControls },
    { "/api/setSplashImage", setSplashImage },
    { "/api/reboot", reboot },
    { "/api/getDisplayOptions", getDisplayOptions },
    { "/api/getGamepadOptions", getGamepadOptions },
    { "/api/getButtonLayoutDefs", getButtonLayoutDefs },
    { "/api/getButtonLayouts", getButtonLayouts },
    { "/api/getLedOptions", getLedOptions },
    { "/api/getPinMappings", getPinMappings },
    { "/api/getProfileOptions", getProfileOptions },
    { "/api/getKeyMappings", getKeyMappings },
    { "/api/getAddonsOptions", getAddonOptions },
    { "/api/getWiiControls", getWiiControls },
    { "/api/getMacroAddonOptions", getMacroAddonOptions },
    { "/api/resetSettings", resetSettings },
    { "/api/getSplashImage", getSplashImage },
    { "/api/getFirmwareVersion", getFirmwareVersion },
    { "/api/getMemoryReport", getMemoryReport },
    { "/api/getHeldPins", getHeldPins },
    { "/api/abortGetHeldPins", abortGetHeldPins },
    { "/api/getUsedPins", getUsedPins },
    { "/api/getConfig", getConfig },
#if !defined(NDEBUG)
    { "/api/echo", echo },
#endif
};

typedef DataAndStatusCode (*HandlerFuncStatusCodePtr)();
static const std::pair<const char*, HandlerFuncStatusCodePtr> handlerFuncsWithStatusCode[] =
{
    { "/api/setConfig", setConfig },
};

int fs_open_custom(struct fs_file *file, const char *name)
{
    for (const auto& handlerFunc : handlerFuncs)
    {
        if (strcmp(handlerFunc.first, name) == 0)
        {
            return set_file_data(file, handlerFunc.second());
        }
    }

    for (const auto& handlerFunc : handlerFuncsWithStatusCode)
    {
        if (strcmp(handlerFunc.first, name) == 0)
        {
            return set_file_data(file, handlerFunc.second());
        }
    }

    for (const char* excludePath : excludePaths)
        if (strcmp(excludePath, name) == 0)
            return 0;

    for (const char* spaPath : spaPaths)
    {
        if (strcmp(spaPath, name) == 0)
        {
            file->data = (const char *)file__index_html[0].data;
            file->len = file__index_html[0].len;
            file->index = file__index_html[0].len;
            file->http_header_included = file__index_html[0].http_header_included;
            file->pextension = NULL;
            file->is_custom_file = 0;
            return 1;
        }
    }

    return 0;
}

void fs_close_custom(struct fs_file *file)
{
    if (file && file->is_custom_file && file->pextension)
    {
        mem_free(file->pextension);
        file->pextension = NULL;
    }
}

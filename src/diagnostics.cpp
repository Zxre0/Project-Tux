#include "fol/core.h"
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QLibrary>
#include <QNetworkInterface>
#include <QSysInfo>
#include <QUdpSocket>
#include <vector>
#include <vulkan/vulkan.h>

namespace fol {
QJsonObject vulkanReport() {
    QLibrary loader("libvulkan.so.1");
    QJsonObject result{{"loader_64", loader.load()},
                       {"devices", QJsonArray{}},
                       {"functional", false},
                       {"loader_32", "unverified"}};
    if (!loader.isLoaded()) {
        result["error"] = loader.errorString();
        return result;
    }
    auto create = reinterpret_cast<PFN_vkCreateInstance>(loader.resolve("vkCreateInstance"));
    auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(loader.resolve("vkDestroyInstance"));
    auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        loader.resolve("vkEnumeratePhysicalDevices"));
    auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        loader.resolve("vkGetPhysicalDeviceProperties"));
    if (!create || !destroy || !enumerate || !properties) {
        result["error"] = "Vulkan entry points missing";
        return result;
    }
    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    VkInstance instance{};
    VkResult status = create(&info, nullptr, &instance);
    result["instance_result"] = int(status);
    if (status != VK_SUCCESS)
        return result;
    uint32_t count = 0;
    status = enumerate(instance, &count, nullptr);
    QJsonArray devices;
    if (status == VK_SUCCESS && count <= 64) {
        std::vector<VkPhysicalDevice> physical(count);
        status = enumerate(instance, &count, physical.data());
        if (status == VK_SUCCESS) {
            for (uint32_t i = 0; i < count; ++i) {
                VkPhysicalDeviceProperties props{};
                properties(physical[i], &props);
                devices.append(QJsonObject{{"name", QString::fromUtf8(props.deviceName)},
                                           {"vendor_id", int(props.vendorID)},
                                           {"device_id", int(props.deviceID)},
                                           {"type", int(props.deviceType)},
                                           {"driver_version_raw", qint64(props.driverVersion)},
                                           {"api_version_raw", qint64(props.apiVersion)}});
            }
        }
    }
    destroy(instance, nullptr);
    result["enumeration_result"] = int(status);
    result["devices"] = devices;
    result["functional"] = !devices.isEmpty();
    for (const QString path :
         {"/usr/lib32/libvulkan.so.1", "/usr/lib/i386-linux-gnu/libvulkan.so.1"}) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            auto header = file.read(5);
            if (header.startsWith(QByteArray("\x7f"
                                             "ELF",
                                             4)) &&
                header.size() == 5 && header[4] == 1)
                result["loader_32"] = "ELF32 present; device functionality untested";
        }
    }
    return result;
}
QJsonObject portReport(const QString &address, quint16 port) {
    QHostAddress host(address);
    require(!host.isNull() &&
                (host.isLoopback() || QNetworkInterface::allAddresses().contains(host)),
            "Bind probe requires a loopback or local interface address");
    QUdpSocket socket;
    bool ok = socket.bind(host, port, QUdpSocket::DontShareAddress);
    return {{"address", address},
            {"port", port},
            {"protocol", "UDP"},
            {"bind_available", ok},
            {"error", ok ? "" : socket.errorString()},
            {"firewall", "not tested"},
            {"upnp", "off"}};
}
QJsonObject systemReport() {
    QJsonObject display;
    for (const char *key :
         {"XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP", "WAYLAND_DISPLAY", "DISPLAY"})
        display[key] = qEnvironmentVariable(key);
    QJsonArray gpu;
    QDir pci("/sys/bus/pci/devices");
    for (const auto &device : pci.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        auto root = pci.filePath(device);
        QFile classFile(root + "/class");
        if (!classFile.open(QIODevice::ReadOnly) ||
            !classFile.readAll().trimmed().startsWith("0x03"))
            continue;
        gpu.append(QJsonObject{
            {"pci_address", device},
            {"vendor", QString::fromUtf8(readFile(root + "/vendor")).trimmed()},
            {"device", QString::fromUtf8(readFile(root + "/device")).trimmed()},
            {"driver", QFileInfo(QFileInfo(root + "/driver").symLinkTarget()).fileName()}});
    }
    QJsonArray eac;
    for (const auto &process : processes())
        if (process.eac)
            eac.append(process.json());
    return {
        {"os", QSysInfo::prettyProductName()},
        {"distribution", QSysInfo::productType()},
        {"kernel", QSysInfo::kernelVersion()},
        {"architecture", QSysInfo::currentCpuArchitecture()},
        {"display", display},
        {"gpus", gpu},
        {"vulkan", vulkanReport()},
        {"runtimes", runtimes()},
        {"eac_processes", eac},
        {"eac_status", eac.isEmpty() ? "not observed in readable process snapshot" : "detected"},
        {"launch_profile", "Private Development"},
        {"server", portReport("127.0.0.1", 7777)},
        {"dxvk", "not configured"},
        {"vkd3d", "not configured"},
        {"reboot_module", "not configured"},
        {"gameplay", "unverified"}};
}
} // namespace fol

#include "AudioDevice.h"
#include "Mmdeviceapi.h"
#include "PolicyConfig.h"
#include "Propidl.h"
#include "Functiondiscoverykeys_devpkey.h"
#include <Windows.h>
#include <endpointvolume.h>
#include <audioclient.h>

bool AudioDevice::isNull()
{
    return id.isEmpty();
}

QString AudioDevice::getPureName()
{
    static auto _getPureName = [](const QString& name) -> QString { //get name in ()
        static QRegExp rx("\\((.+)\\)");
        if (rx.indexIn(name) != -1)
            return rx.cap(1);
        return "";
    };
    return _getPureName(this->name);
}

bool AudioDevice::setDefaultOutputDevice(QString devID)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); //单线程并发
    if (FAILED(hr)) return false;
    IPolicyConfigVista* pPolicyConfig;
    ERole reserved = eConsole;

    hr = CoCreateInstance(__uuidof(CPolicyConfigVistaClient), NULL, CLSCTX_ALL, __uuidof(IPolicyConfigVista), (LPVOID*)&pPolicyConfig);
    if (SUCCEEDED(hr)) {
        hr = pPolicyConfig->SetDefaultEndpoint(devID.toStdWString().c_str(), reserved);
        pPolicyConfig->Release();
    }
    ::CoUninitialize();
    return SUCCEEDED(hr);
}

QList<AudioDevice> AudioDevice::enumOutputDevice()
{
    QList<AudioDevice> res;
    HRESULT hr = CoInitialize(NULL); // == CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return res;
    IMMDeviceEnumerator* pEnum = NULL;
    // Create a multimedia device enumerator.
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    if (FAILED(hr)) return res;
    IMMDeviceCollection* pDevices;
    // Enumerate the output devices.
    hr = pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices); //eRender == out; eCapture == in
    if (FAILED(hr)) return res;
    UINT count;
    hr = pDevices->GetCount(&count);
    if (FAILED(hr)) return res;
    for (UINT i = 0; i < count; i++) {
        IMMDevice* pDevice;
        hr = pDevices->Item(i, &pDevice);
        if (SUCCEEDED(hr)) {
            LPWSTR wstrID = NULL;
            hr = pDevice->GetId(&wstrID);
            if (SUCCEEDED(hr)) {
                IPropertyStore* pStore;
                hr = pDevice->OpenPropertyStore(STGM_READ, &pStore);
                if (SUCCEEDED(hr)) {
                    PROPVARIANT friendlyName;
                    PropVariantInit(&friendlyName);
                    hr = pStore->GetValue(PKEY_Device_FriendlyName, &friendlyName);
                    if (SUCCEEDED(hr)) {
                        res << AudioDevice(QString::fromWCharArray(wstrID), QString::fromWCharArray(friendlyName.pwszVal));
                        PropVariantClear(&friendlyName);
                    }
                    pStore->Release();
                }
            }
            pDevice->Release();
        }
    }
    pDevices->Release();
    pEnum->Release();
    ::CoUninitialize();
    return res;
}

AudioDevice AudioDevice::defaultOutputDevice()
{
    AudioDevice res;
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) return res;
    IMMDeviceEnumerator* pEnum = NULL;
    // Create a multimedia device enumerator.
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    if (FAILED(hr)) return res;
    // Enumerate the output devices.
    IMMDevice* pDevice;
    hr = pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice); //eRender == out; eCapture == in
    if (FAILED(hr)) return res;
    LPWSTR wstrID = NULL;
    hr = pDevice->GetId(&wstrID);
    if (SUCCEEDED(hr)) {
        IPropertyStore* pStore;
        hr = pDevice->OpenPropertyStore(STGM_READ, &pStore);
        if (SUCCEEDED(hr)) {
            PROPVARIANT friendlyName;
            PropVariantInit(&friendlyName);
            hr = pStore->GetValue(PKEY_Device_FriendlyName, &friendlyName);
            if (SUCCEEDED(hr)) {
                res.id = QString::fromWCharArray(wstrID);
                res.name = QString::fromWCharArray(friendlyName.pwszVal);
                PropVariantClear(&friendlyName);
            }
            pStore->Release();
        }
    }
    pDevice->Release();
    pEnum->Release();
    ::CoUninitialize();
    return res;
}

bool AudioDevice::setVolume(int volume)
{
    bool ret = false;
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) return false;
    IMMDeviceEnumerator* pDeviceEnumerator = 0;
    IMMDevice* pDevice = 0;
    IAudioEndpointVolume* pAudioEndpointVolume = 0;
    IAudioClient* pAudioClient = 0;

    try {
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pDeviceEnumerator);
        if (FAILED(hr)) throw "CoCreateInstance";
        hr = pDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
        if (FAILED(hr)) throw "GetDefaultAudioEndpoint";
        hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pAudioEndpointVolume);
        if (FAILED(hr)) throw "pDevice->Active";
        hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
        if (FAILED(hr)) throw "pDevice->Active";

        float fVolume;
        fVolume = volume / 100.0f;
        hr = pAudioEndpointVolume->SetMasterVolumeLevelScalar(fVolume, &GUID_NULL);
        if (FAILED(hr)) throw "SetMasterVolumeLevelScalar";

        pAudioClient->Release();
        pAudioEndpointVolume->Release();
        pDevice->Release();
        pDeviceEnumerator->Release();

        ret = true;
    } catch (...) {
        if (pAudioClient) pAudioClient->Release();
        if (pAudioEndpointVolume) pAudioEndpointVolume->Release();
        if (pDevice) pDevice->Release();
        if (pDeviceEnumerator) pDeviceEnumerator->Release();
        throw;
    }

    ::CoUninitialize();
    return ret;
}

int AudioDevice::getVolume()
{
    int volumeValue = 0;
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) return -1;
    IMMDeviceEnumerator* pDeviceEnumerator = 0;
    IMMDevice* pDevice = 0;
    IAudioEndpointVolume* pAudioEndpointVolume = 0;
    IAudioClient* pAudioClient = 0;

    try {
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pDeviceEnumerator);
        if (FAILED(hr)) throw "CoCreateInstance";
        hr = pDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
        if (FAILED(hr)) throw "GetDefaultAudioEndpoint";
        hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pAudioEndpointVolume);
        if (FAILED(hr)) throw "pDevice->Active";
        hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
        if (FAILED(hr)) throw "pDevice->Active";

        float fVolume;
        hr = pAudioEndpointVolume->GetMasterVolumeLevelScalar(&fVolume);
        if (FAILED(hr)) throw "SetMasterVolumeLevelScalar";

        pAudioClient->Release();
        pAudioEndpointVolume->Release();
        pDevice->Release();
        pDeviceEnumerator->Release();

        volumeValue = fVolume * 100;
    } catch (...) {
        if (pAudioClient) pAudioClient->Release();
        if (pAudioEndpointVolume) pAudioEndpointVolume->Release();
        if (pDevice) pDevice->Release();
        if (pDeviceEnumerator) pDeviceEnumerator->Release();
        throw;
    }

    ::CoUninitialize(); //不安全 没有考虑抛出异常！！！
    return volumeValue;
}

QDebug operator<<(QDebug dbg, const AudioDevice& dev)
{
    dbg << dev.id << " " << dev.name;
    return dbg;
}
bool operator==(const AudioDevice& lhs, const AudioDevice& rhs)
{
    return lhs.id == rhs.id;
}

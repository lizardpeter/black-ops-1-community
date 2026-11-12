#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <cstdio>
#include <memory>

using Microsoft::WRL::ComPtr;

namespace
{
HRESULT ProbeDefaultEndpoint()
{
    ComPtr<IMMDeviceEnumerator> devices;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(devices.GetAddressOf()));
    if (FAILED(hr))
        return hr;

    ComPtr<IMMDevice> endpoint;
    hr = devices->GetDefaultAudioEndpoint(eRender, eConsole, endpoint.GetAddressOf());
    if (FAILED(hr))
        return hr;

    ComPtr<IAudioClient3> client;
    hr = endpoint->Activate(__uuidof(IAudioClient3), CLSCTX_INPROC_SERVER, nullptr,
                            reinterpret_cast<void **>(client.GetAddressOf()));
    if (FAILED(hr))
        return hr;

    WAVEFORMATEX *format = nullptr;
    hr = client->GetMixFormat(&format);
    if (FAILED(hr))
        return hr;
    const std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> mixFormat(format, CoTaskMemFree);

    UINT32 defaultPeriod, fundamentalPeriod, minimumPeriod, maximumPeriod;
    hr = client->GetSharedModeEnginePeriod(format, &defaultPeriod, &fundamentalPeriod,
                                           &minimumPeriod, &maximumPeriod);
    if (FAILED(hr))
        return hr;

    std::printf("Endpoint: %lu Hz, %u channels\n", format->nSamplesPerSec, unsigned(format->nChannels));
    std::printf("Shared periods in frames: min %u, default %u, step %u, max %u\n",
                minimumPeriod, defaultPeriod, fundamentalPeriod, maximumPeriod);
    hr = client->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                             minimumPeriod, format, nullptr);
    if (FAILED(hr))
        return hr;

    UINT32 bufferFrames;
    hr = client->GetBufferSize(&bufferFrames);
    if (SUCCEEDED(hr))
        std::printf("Accepted minimum period: %.3f ms. Endpoint buffer: %u frames.\n",
                    1000.0 * minimumPeriod / format->nSamplesPerSec, bufferFrames);
    return hr;
}
}

int main()
{
    static_assert(sizeof(void *) == 8);
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        hr = ProbeDefaultEndpoint();
        CoUninitialize();
    }
    if (FAILED(hr))
        std::fprintf(stderr, "WASAPI endpoint probe failed (HRESULT 0x%08lX)\n",
                     static_cast<unsigned long>(hr));
    return FAILED(hr) ? 1 : 0;
}

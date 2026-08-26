# MojaStacja

Firmware tablicy najbliższych odjazdów dla ESP32 i ekranów dotykowych.

## Uruchomienie — 2 minuty

1. Skopiuj `include/operation/fw_station_config_local.h.example` jako `include/operation/fw_station_config_local.h`.
2. Ustaw Wi-Fi; opcjonalnie ustaw `FW_STOP_QUERY`, aby zawęzić pobieraną listę przystanków.
3. Zbuduj program: `pio run -e m5stack-cores3` albo `pio run -e m5stack-tab5`.

Przytrzymaj ekran przez 3 sekundy, aby otworzyć listę miast. Puść ekran, stuknij miasto, a następnie stuknij przystanek pobrany po Wi-Fi. Listę przystanków przewija się ruchem palca góra–dół; pozostaje w pamięci do ponownego wyboru Gdańska. W prawym górnym rogu `WiFi -NN` oznacza połączenie i siłę sygnału w dBm; czerwone `WiFi OFF` oznacza brak sieci.

## Architektura

`operation/` nie zna M5Stack, Arduino ani konkretnego wyświetlacza:

- `fw_transit_source_t` — wspólny kontrakt dla każdego miasta;
- `driver_http_client_t` — kontrakt HTTP, bez zależności od Wi-Fi;
- `ui_display_t` — małe API rysowania, kompatybilne z adapterem Adafruit GFX/M5GFX;
- `sys_platform.h` — jedyny kontrakt płyty. Kod M5 jest wyłącznie w `src/systems/sys_m5stack_platform.cpp`.

Dodanie Waveshare wymaga nowego pliku implementującego `sys_platform.h`; nie wymaga importowania API M5 do UI ani do źródeł danych.

## Źródła miast

Katalog źródeł jest w `src/operation/fw_city_catalogue.cpp`.

| Miasto | Format źródłowy | Stan adaptera |
| --- | --- | --- |
| Gdańsk | TRISTAR JSON | Gotowy: bieżące odjazdy z `departures?stopId=` |
| Warszawa | API otwartych danych | Wymaga klucza API i adaptera zasobu wybranego z katalogu |
| Łódź | GTFS i GTFS-RT protobuf | Wymaga dekodera GTFS-RT oraz lokalnego indeksu przystanków |
| Wrocław | statyczny GTFS ZIP | Wymaga importu GTFS do pamięci flash/SD |
| Poznań | statyczny GTFS ZIP | Wymaga importu GTFS do pamięci flash/SD |

Gdańsk jest jedynym miastem z jednym publicznym endpointem odjazdów na słupek, więc działa bez pośrednika i bez klucza. Pozostałe miasta zostały wpisane do katalogu z oficjalnymi endpointami, ale ich różne formaty celowo nie są udawane jako jeden niedziałający parser.

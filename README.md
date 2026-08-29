# MojaStacja

Firmware tablicy najbliższych odjazdów dla ESP32 i ekranów dotykowych. Dane pobiera wyłącznie z lokalnego serwera [IOT Open API](../IOTOpenAPI/README.md); płyta nie łączy się bezpośrednio z publicznymi serwisami komunikacji.

## Uruchomienie — 5 minut

1. Uruchom serwer w sieci lokalnej, np. `iot-open-api --provider gdansk`.
2. Wybierz konfigurację: skopiuj `include/secrets.h.example` jako `include/secrets.h` **albo** umieść `config.json` w katalogu głównym karty microSD.
3. Ustaw Wi-Fi i adres serwera zgodnie z przykładem poniżej.
4. Zbuduj program: `pio run -e m5stack-cores3` albo `pio run -e m5stack-tab5`.
5. Przytrzymaj ekran przez 3 sekundy, wybierz miasto, wpisz fragment nazwy przystanku i stuknij `SZUKAJ`.

`include/secrets.h` jest ignorowany przez Git. Gdy oba źródła istnieją, niepuste pola z `/config.json` zastępują wartości z `secrets.h`.

## Konfiguracja

`include/secrets.h` zawiera te same trzy wartości i jest wygodny podczas programowania:

```cpp
#define SECRETS_WIFI_SSID "moja-siec"
#define SECRETS_WIFI_PASSWORD "moje-haslo"
#define SECRETS_PROVIDER_URL "http://192.168.1.50:8000"
```

Alternatywnie karta microSD (FAT32) może zawierać `/config.json`:

```json
{
  "wifi": {
    "ssid": "moja-siec",
    "password": "moje-haslo"
  },
  "provider_url": "http://192.168.1.50:8000"
}
```

`provider_url` to adres bazowy serwera, bez `/transit` na końcu. Obsługiwane są gniazda microSD w Core S3 SE i Tab5. Brak karty lub pliku jest bezpieczny: firmware używa wtedy wartości z `secrets.h`, a przy pustej konfiguracji pokazuje `WiFi OFF`.

## Wybór przystanku

- Tab5 pokazuje pełną klawiaturę QWERTY na ekranie.
- Core S3 pokazuje klawiaturę telefoniczną: kolejne stuknięcia tego samego pola przez 0,9 s przełączają literę (`2 ABC`, `3 DEF` itd.).
- `WROC` wraca do miast, `USUN` kasuje ostatni znak, a `SZUKAJ` pobiera maksymalnie 255 pasujących przystanków z lokalnego API.

Po wyborze przystanku ekran odświeża odjazdy co 30 sekund. CoreS3 pobiera pięć pozycji — dokładnie tyle, ile mieści ekran — a większe ekrany pobierają liczbę pozycji, które mogą wyświetlić. W nagłówku `WiFi` ma kolor zależny od RSSI: zielony (silny), żółty, pomarańczowy lub czerwony (słaby/brak połączenia). `Srv` jest zielone po udanym połączeniu z serwerem, czerwone gdy serwer nie odpowiada.

Lista miast jest pobierana przy starcie i co 30 sekund z `GET /status`. Aktualne API zwraca slugi providerów, więc są one używane jako nazwy, chyba że odpowiedź zawiera opcjonalne pole `city`. Długie listy miast i przystanków mają strony; użyj przycisków `<` i `>` na dole ekranu.

Nazwy miast, przystanków, kierunków i wybranego przystanku przewijają się poziomo, gdy nie mieszczą się na ekranie. Czas jest synchronizowany z NTP i wyświetlany według strefy Polski (`CET`/`CEST`); odjazdy są pobierane w zadaniu tła, więc poprzedni rozkład pozostaje widoczny podczas odświeżania.

## Architektura

`operation/` nie zna M5Stack, Arduino ani konkretnego wyświetlacza:

- `fw_local_api_source_t` — adapter jednolitego lokalnego API dla każdego miasta;
- `driver_http_client_t` — kontrakt HTTP, bez zależności od Wi-Fi;
- `ui_display_t` — małe API rysowania, kompatybilne z adapterem Adafruit GFX/M5GFX;
- `sys_platform.h` — jedyny kontrakt płyty. Kod M5 i odczyt karty SD są wyłącznie w `src/systems/sys_m5stack_platform.cpp`.

Adresy używane przez firmware:

```text
GET /transit/{provider}/stops?query={fragment}
GET /transit/{provider}/schedule/{stop_name}/{visible_count}
GET /status
```

`/status` zwraca listę providerów aktywnych na danym serwerze, więc firmware nie utrzymuje własnej, stałej listy miast.

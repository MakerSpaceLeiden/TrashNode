ThrashNode
==========

- Node heeft een "beeld" waar een trash-bin IS:
  - Binnen (op zijn normale plek)
  - Buiten (voor legen)
  - "Zoek" (niet op de goede plek, niet buiten om te legen)
- en een idee waar de bin zou MOETEN zijn:
  - De trash-bin moet naar binnen (na leging)
  - De trash-bin moet naar buiten (om geleegd te worden)
  - (Uiteraard is er geen geval: "De trash-bin moet ZOEK worden")
  
PoC
===

De meeste simpele implementatie is 
- Een paar LED's die de huidige en de gewenste positie aangeven (waaruit ook blijkt dat ACTIE gewenst is)
- Die actie wordt ondernomen door een persoon, die het uitvoeren ervan bevestigt met een druk op één van enkele knoppen:
  - "Ik 'm buiten gezet"
  - "Ik heb 'm binnen gezet"
  - "Hij was zoek (stond niet op de goede plaats) en ik heb 'm opgesnord en op de juist plek gezet"
  - "Hij is zoek !!1!"
- Er komt (periodiek) een signaal van een backend-systeem omtrent de gewenste positie van de trash-bin:
  - "Hij moet (vóór morgenochtend) buiten staan"
  - "Hij is (als het goed) geleegd en moet naar binnen"

Vervolg (MVP?)
==============
- Een 'swipe' waarmee de uitvoerder van de actie wordt geïdentificeerd en/of geautoriseerd
- Een rapportage van de 'state' terug naar het backend-systeem

Build
=====

Dit compileert tegen de ACNode arduino-library uit: ```git@github.com:MakerSpaceLeiden/AccesSystem.git``` (directory: ```lib-arduino/ACNode```, zie ook de README.md aldaar), met de volgende kanttekeningen:

- Oorspronkelijk was het nodig om de laatste 1.0.x-versie van de Espressif esp32 software te installeren (versie 1.0.6), er was een probleem in de ACNode-code die gebruik van 2.0.x verhinderende. Dat zou gefixed moeten zijn, maar dat is nog niet getest.
- In de trashnode wordt een aangepaste verie van RFID.{h|cpp} gebruikt ("MyRFID"), voor gebruik van een PN532 via i2c zonder reset en zonder irq. Dat zou misschien nog eens naar upstream gemerged kunnen worden.
- In ```/Arduino/libraries/PubSubClient/src/PubSubClient.h``` moet ```#define MQTT_MAX_PACKET_SIZE 256``` aangepast worden in ```#define MQTT_MAX_PACKET_SIZE 1024```. Daar zijn vast betere oplossingen voor. Alléén maar een ```#define``` in de code van de trashnode zelf is in ieder geval niet voldoende.



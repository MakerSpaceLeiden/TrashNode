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





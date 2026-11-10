# Keuzeknoppen (Select buttons)

Een rij knoppen voor een `input_select` of een `select` in Home Assistant: een knop per optie, de huidige optie licht
op. Tik op een knop om die optie te kiezen. Gemaakt om een HDMI-ingang tussen een paar bronnen te wisselen, maar het
werkt voor elke keuze uit een handvol opties: een scène, een modus, een speaker.

[English version](README.md)

- Elke optie van de entity krijgt een knop, tot zes. Een brede tegel zet ze naast elkaar, een hoge stapelt ze, een
  vierkante maakt er een raster van.
- De huidige optie is de knop die oplicht. Verandert de optie ergens anders (een automatisering, de Home Assistant-app,
  een afstandsbediening), dan volgt het scherm.
- Na een tik krijgt de knop een rand tot Home Assistant de nieuwe optie heeft; dan licht hij op.
- Geef elke knop desgewenst een icoon; met **Namen van de opties tonen** uit staan alleen de iconen er.
- Zet **Twee keer tikken om te wisselen** aan en de eerste tik vraagt "Tik nog eens": tegen per ongeluk wegschakelen.

## Wat je nodig hebt

- Tessera 0.4.91 of nieuwer.
- Een `input_select`-helper of een `select`-entity in Home Assistant met de opties die je wilt, zoals `Devkit`,
  `Personal PC` en `Office PC`.

## Instellen

1. Voeg de plugin toe aan een scherm in de Tessera-app.
2. Zet in Layout de tegel **Keuzeknoppen** uit de pluginbibliotheek neer, in de grootte die je wilt (drie breed is een
   3-wegschakelaar).
3. Kies in de inspector de `input_select` of `select` als entity van de tegel.
4. Kies eventueel een icoon per optie (in de volgorde waarin Home Assistant ze noemt), zet **Naam tonen** of **Namen
   van de opties tonen** uit, of zet **Twee keer tikken om te wisselen** aan.
5. Bouw het scherm en installeer het.

## Goed om te weten

- De knoppen sturen `input_select.select_option` of `select.select_option`; het scherm moet acties mogen uitvoeren.
- Op een 1x1-tegel staan drie knoppen naast elkaar en lange namen worden ingekort: geef ze iconen, of neem een
  bredere tegel.
- Een entity met meer dan zes opties toont de eerste zes. Tessera's eigen tegel voor de entity heeft ze allemaal.
- Een optie van meer dan 45 tekens komt ingekort aan; de knop staat er wel, maar kan hem niet kiezen.

## Op welke schermen geprobeerd

Nog op geen enkel scherm. Alleen gecontroleerd tegen de code van Tessera en op een computer getekend.

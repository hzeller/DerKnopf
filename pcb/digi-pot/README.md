DigiPot
=======

The actual simulated pot with connectors for the LED ring, the IR receiver and
a local optical rotational encoder to have the feel of a local potentiometer.

This encoder replaces an existing high-quality ALPS potentiometer with 30 detents.
It is surprisingly hard to find a mechanical encoder with the same solid feel
and 32 detents (most mechanical enoders only go to 20 steps/turn).

The CUI optical encoder is perfect for this job but about as expensive as the
potentiometer it replaces.

## BOM

 * 2x 4.7k 0603 resistor
 * 4x 100n 0603 capacitor
 * 2x (4x75 Ohm) resistor network.
 * 1x Attiny48
 * 1x DS1882
 * 1x CUI C14D32P-A3 optical encoder.
 * 1x FlatFlex 12Pin connector.

![](../../img/digi-pot-schem.png)
![](../../img/digi-pot-render.png)

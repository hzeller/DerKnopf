Remote Control
==============

The remote control, operated with an Attiny44 on a CR2032 battery. Processor
gets woken up from power down state on state change of the inputs, so quiescent
current is less than 0.2 μA.

With optional 1M pullups for the mechanical encoder, as these sometimes get
stuck in some half position, draining current through the pull-ups. The
standard pullups inside the attiny are aorund 60k (Note: this feature is not
used in the assembly below, just using the internal resistors.)

The shown programming header does not have a component associated on the board
as it was too bulky.

Single-sided board for simple home-etching.

Top                    | Side                       | Bottom
-----------------------|----------------------------|--------------------------
![](../../img/sender-top.jpg)|![](../../img/sender-sideways.jpg)|![](../../img/sender-bottom.jpg)

![](../../img/remote-control-schem.png)

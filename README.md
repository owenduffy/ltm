# Logging temperature meter (ltm)    
Repository for code related to project described at See https://owenduffy.net/blog/?s=ltm

In addition to libraries available under Arduino Library Manager, the project uses the following:
* [NewLiquidCrystal](https://github.com/fmalpartida/New-LiquidCrystal/wiki)
* [LcdBarGraphX (modified)](https://github.com/owenduffy/LcdBarGraphX)
 
See discussion about [I2C LCD configuration](https://owenduffy.net/blog/?s=LiquidCrystal_I2C+type).

# USB chip
The prototype used a nodeMCU v1.0 dev board with CP210x USB chip. The boards are available with other USB chips, but I would avoid ANY Prolific chips and CH340 chips for driver compatibility reasons.
 FTDI spoiled their reputation by disabling clones and there are lots of cloned FTDI chips out of China that make them a risky future.

Copyright: Owen Duffy 2021/04/14.



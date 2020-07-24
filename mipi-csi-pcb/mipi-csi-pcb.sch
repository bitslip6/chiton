EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "MIPI-CSI interface for Variscite EVK"
Date "2020-03-23"
Rev "1.0"
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L Connector_Generic:Conn_02x30_Odd_Even J2
U 1 1 5E791870
P 6950 3500
F 0 "J2" H 7000 5117 50  0000 C CNN
F 1 "Variscite_Edge" H 7000 5026 50  0000 C CNN
F 2 "mipi-csi-pcb:snapeda-HSEC8-130-01-L-DV-A" H 6950 3500 50  0001 C CNN
F 3 "~" H 6950 3500 50  0001 C CNN
	1    6950 3500
	1    0    0    -1  
$EndComp
Text Notes 6750 5200 0    50   ~ 0
Bottom - Top
Wire Wire Line
	3350 2450 3100 2450
Wire Wire Line
	3100 2450 3100 2750
Wire Wire Line
	3350 2750 3100 2750
Connection ~ 3100 2750
Wire Wire Line
	3100 2750 3100 3050
Wire Wire Line
	3350 3050 3100 3050
Connection ~ 3100 3050
Wire Wire Line
	3100 3050 3100 3350
Wire Wire Line
	3350 3350 3100 3350
Connection ~ 3100 3350
Wire Wire Line
	3100 3350 3100 4000
Text Label 2250 2550 0    50   ~ 0
MIPI_CSI_DATA0_N
Text Label 2250 2650 0    50   ~ 0
MIPI_CSI_DATA0_P
Text Label 2250 2850 0    50   ~ 0
MIPI_CSI_DATA1_N
Text Label 2250 2950 0    50   ~ 0
MIPI_CSI_DATA1_P
Text Label 2250 3150 0    50   ~ 0
MIPI_CSI_CLK_N
Text Label 2250 3250 0    50   ~ 0
MIPI_CSI_CLK_P
Text Label 6250 2600 2    50   ~ 0
MIPI_CSI_DATA0_P
Text Label 6250 2700 2    50   ~ 0
MIPI_CSI_DATA0_N
Text Label 6250 3000 2    50   ~ 0
MIPI_CSI_CLK_N
Text Label 6250 2900 2    50   ~ 0
MIPI_CSI_CLK_P
Text Label 6300 3300 2    50   ~ 0
MIPI_CSI_DATA1_N
Text Label 6300 3200 2    50   ~ 0
MIPI_CSI_DATA1_P
Text Notes 9650 5500 0    50   ~ 0
Layout note:\nDifferential impedance: 100 ohms
NoConn ~ 7450 2500
NoConn ~ 7450 2600
NoConn ~ 7450 2700
NoConn ~ 7450 2800
NoConn ~ 7450 3000
NoConn ~ 7450 3300
NoConn ~ 6550 3800
NoConn ~ 6550 3900
NoConn ~ 6550 4100
NoConn ~ 6550 4200
NoConn ~ 7450 4700
NoConn ~ 7450 4800
NoConn ~ 7450 4900
NoConn ~ 7450 5000
NoConn ~ 6550 5000
NoConn ~ 6550 4700
NoConn ~ 6550 4600
NoConn ~ 6550 4500
NoConn ~ 6550 4400
NoConn ~ 6550 4300
Wire Wire Line
	6550 3800 6750 3800
Wire Wire Line
	6550 3900 6750 3900
Wire Wire Line
	6550 4100 6750 4100
Wire Wire Line
	6550 4200 6750 4200
Wire Wire Line
	6550 4300 6750 4300
Wire Wire Line
	6550 4400 6750 4400
Wire Wire Line
	6550 4500 6750 4500
Wire Wire Line
	6550 4600 6750 4600
Wire Wire Line
	6550 4700 6750 4700
Wire Wire Line
	6550 5000 6750 5000
Wire Wire Line
	7450 2500 7250 2500
Wire Wire Line
	7450 2600 7250 2600
Wire Wire Line
	7450 2700 7250 2700
Wire Wire Line
	7450 2800 7250 2800
Wire Wire Line
	7450 3000 7250 3000
Wire Wire Line
	7450 3300 7250 3300
Wire Wire Line
	7450 4700 7250 4700
Wire Wire Line
	7450 4800 7250 4800
Wire Wire Line
	7450 4900 7250 4900
Wire Wire Line
	7450 5000 7250 5000
Text Label 6300 4800 2    50   ~ 0
i2c_scl
Text Label 6300 4900 2    50   ~ 0
i2c_sda
Text Label 2950 3650 2    50   ~ 0
i2c_scl
Text Label 2950 3750 2    50   ~ 0
i2c_sda
Text Label 5800 2100 2    50   ~ 0
3.3
Text Label 2950 3850 2    50   ~ 0
3.3
Wire Wire Line
	2950 3850 3350 3850
$Comp
L Connector:Conn_01x04_Male J4
U 1 1 5E8F61AB
P 6250 2200
F 0 "J4" H 6358 2481 50  0000 C CNN
F 1 "3.3_1.8" H 6358 2390 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical" H 6250 2200 50  0001 C CNN
F 3 "~" H 6250 2200 50  0001 C CNN
	1    6250 2200
	1    0    0    -1  
$EndComp
Wire Wire Line
	6450 2200 6600 2200
Wire Wire Line
	6600 2200 6600 2100
Wire Wire Line
	6600 2100 6750 2100
Connection ~ 6600 2200
Wire Wire Line
	6600 2200 6750 2200
Wire Wire Line
	6450 2400 6600 2400
Wire Wire Line
	6600 2400 6600 2300
Wire Wire Line
	6600 2300 6750 2300
Connection ~ 6600 2400
Wire Wire Line
	6600 2400 6750 2400
Wire Wire Line
	7250 4600 7450 4600
NoConn ~ 7450 4600
$Comp
L Connector:Conn_01x02_Male J3
U 1 1 5E9BE34C
P 5300 4950
F 0 "J3" V 5362 4994 50  0000 L CNN
F 1 "CSI1_GND" V 5453 4994 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical" H 5300 4950 50  0001 C CNN
F 3 "~" H 5300 4950 50  0001 C CNN
	1    5300 4950
	0    1    1    0   
$EndComp
Wire Wire Line
	5300 5150 6400 5150
Wire Wire Line
	6400 3400 6750 3400
Wire Wire Line
	6400 3100 6750 3100
Wire Wire Line
	6400 3100 6400 3400
Connection ~ 6400 3400
Wire Wire Line
	6400 3400 6400 3700
Wire Wire Line
	6750 2500 6400 2500
Wire Wire Line
	6400 2500 6400 2800
Connection ~ 6400 3100
Wire Wire Line
	6400 2800 6750 2800
Connection ~ 6400 2800
Wire Wire Line
	6400 2800 6400 3100
Wire Wire Line
	3100 4000 3500 4000
$Comp
L Connector:Conn_01x10_Male J5
U 1 1 5EAB7751
P 4950 1700
F 0 "J5" V 4785 1628 50  0000 C CNN
F 1 "CSI1" V 4876 1628 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x10_P2.54mm_Vertical" H 4950 1700 50  0001 C CNN
F 3 "~" H 4950 1700 50  0001 C CNN
	1    4950 1700
	0    1    1    0   
$EndComp
$Comp
L power:GNDA #PWR01
U 1 1 5EADD6ED
P 3100 4000
F 0 "#PWR01" H 3100 3750 50  0001 C CNN
F 1 "GNDA" H 3105 3827 50  0000 C CNN
F 2 "" H 3100 4000 50  0001 C CNN
F 3 "" H 3100 4000 50  0001 C CNN
	1    3100 4000
	1    0    0    -1  
$EndComp
Connection ~ 3100 4000
$Comp
L power:GNDD #PWR02
U 1 1 5EAEE8EF
P 6400 5150
F 0 "#PWR02" H 6400 4900 50  0001 C CNN
F 1 "GNDD" H 6404 4995 50  0000 C CNN
F 2 "" H 6400 5150 50  0001 C CNN
F 3 "" H 6400 5150 50  0001 C CNN
	1    6400 5150
	1    0    0    -1  
$EndComp
Connection ~ 6400 5150
Wire Wire Line
	5800 2100 6150 2100
Wire Wire Line
	6150 2100 6150 2300
Wire Wire Line
	6150 2300 6450 2300
Connection ~ 6150 2100
Wire Wire Line
	6150 2100 6450 2100
Wire Wire Line
	6750 3700 6400 3700
Connection ~ 6400 3700
Wire Wire Line
	6400 3700 6400 4000
Wire Wire Line
	6750 4000 6400 4000
Connection ~ 6400 4000
Wire Wire Line
	6400 4000 6400 5150
Wire Wire Line
	4750 1900 4750 3500
Wire Wire Line
	4750 3500 6750 3500
Wire Wire Line
	6750 3600 4650 3600
Wire Wire Line
	4650 3600 4650 1900
Wire Wire Line
	6750 2700 5350 2700
Text Label 6300 3500 2    50   ~ 0
MIPI_CSI_DATA2_P
Text Label 6300 3600 2    50   ~ 0
MIPI_CSI_DATA2_N
Wire Wire Line
	4550 1900 4550 4800
Wire Wire Line
	4550 4800 6750 4800
Wire Wire Line
	4450 4900 4450 1900
Wire Wire Line
	4450 4900 6750 4900
$Comp
L Connector:Conn_01x10_Male J7
U 1 1 5F047EC7
P 8950 1700
F 0 "J7" V 8785 1628 50  0000 C CNN
F 1 "CSI2" V 8876 1628 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x10_P2.54mm_Vertical" H 8950 1700 50  0001 C CNN
F 3 "~" H 8950 1700 50  0001 C CNN
	1    8950 1700
	0    1    1    0   
$EndComp
Text Label 7750 2200 0    50   ~ 0
i2c4_sda
Text Label 7750 2300 0    50   ~ 0
i2c4_scl
Wire Wire Line
	7250 3200 7450 3200
NoConn ~ 7450 3200
Wire Wire Line
	8650 3500 8650 1900
Wire Wire Line
	7250 3500 8650 3500
Wire Wire Line
	8750 3600 8750 1900
Wire Wire Line
	7250 3600 8750 3600
Wire Wire Line
	8850 3800 8850 1900
Wire Wire Line
	7250 3800 8850 3800
Wire Wire Line
	8950 3900 8950 1900
Wire Wire Line
	7250 3900 8950 3900
Wire Wire Line
	9050 4100 9050 1900
Wire Wire Line
	7250 4100 9050 4100
Wire Wire Line
	9150 4200 9150 1900
Wire Wire Line
	7250 4200 9150 4200
Wire Wire Line
	7450 4300 7250 4300
NoConn ~ 7450 4300
Wire Wire Line
	9250 4400 9250 1900
Wire Wire Line
	7250 4400 9250 4400
Wire Wire Line
	9350 4500 9350 1900
Wire Wire Line
	7250 4500 9350 4500
Text Label 7750 3500 0    50   ~ 0
csi_p2_data2_N
Text Label 7750 3600 0    50   ~ 0
csi_p2_data2_P
Text Label 7750 3800 0    50   ~ 0
csi_p2_data1_N
Text Label 7750 3900 0    50   ~ 0
csi_p2_data1_P
Text Label 7750 4100 0    50   ~ 0
csi_p2_clk_N
Text Label 7750 4200 0    50   ~ 0
csi_p2_clk_P
Text Label 7750 4400 0    50   ~ 0
csi_p2_data0_N
Text Label 7750 4500 0    50   ~ 0
csi_p2_data0_P
Wire Wire Line
	7250 2300 8550 2300
Wire Wire Line
	8550 1900 8550 2300
Wire Wire Line
	7250 2200 8450 2200
Wire Wire Line
	8450 1900 8450 2200
Wire Wire Line
	7250 2100 7600 2100
Wire Wire Line
	7600 2100 7600 2400
Wire Wire Line
	7600 5200 8400 5200
Wire Wire Line
	8400 5200 8400 5100
Wire Wire Line
	7250 2400 7600 2400
Connection ~ 7600 2400
Wire Wire Line
	7600 2400 7600 2900
Wire Wire Line
	7250 2900 7600 2900
Connection ~ 7600 2900
Wire Wire Line
	7600 2900 7600 3100
Wire Wire Line
	7250 3100 7600 3100
Connection ~ 7600 3100
Wire Wire Line
	7600 3100 7600 3400
Wire Wire Line
	7250 3400 7600 3400
Connection ~ 7600 3400
$Comp
L Connector:Conn_01x02_Male J6
U 1 1 5F179709
P 8500 4900
F 0 "J6" V 8562 4944 50  0000 L CNN
F 1 "CSI2_GND" V 8653 4944 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical" H 8500 4900 50  0001 C CNN
F 3 "~" H 8500 4900 50  0001 C CNN
	1    8500 4900
	0    1    1    0   
$EndComp
Wire Wire Line
	5150 5150 5150 5450
Wire Wire Line
	5150 5450 8500 5450
Wire Wire Line
	8500 5450 8500 5100
Connection ~ 5150 5150
Wire Wire Line
	5150 5150 5200 5150
$Comp
L Connector_Generic:Conn_02x15_Counter_Clockwise J1
U 1 1 5E797A4C
P 3550 3150
F 0 "J1" H 3600 4067 50  0000 C CNN
F 1 "AFE" H 3600 3976 50  0000 C CNN
F 2 "mipi-csi-pcb:117342485" H 3550 3150 50  0001 C CNN
F 3 "~" H 3550 3150 50  0001 C CNN
	1    3550 3150
	1    0    0    -1  
$EndComp
Wire Wire Line
	2150 2550 2150 2250
Wire Wire Line
	2150 2550 3350 2550
Wire Wire Line
	2050 2650 2050 2250
Wire Wire Line
	2050 2650 3350 2650
Wire Wire Line
	1950 2850 1950 2250
Wire Wire Line
	1950 2850 3350 2850
Wire Wire Line
	1850 2950 1850 2250
Wire Wire Line
	1850 2950 3350 2950
Wire Wire Line
	1750 3150 1750 2250
Wire Wire Line
	1750 3150 3350 3150
Wire Wire Line
	1650 3250 1650 2250
Wire Wire Line
	1650 3250 3350 3250
Wire Wire Line
	1550 3450 1550 2250
Wire Wire Line
	1550 3450 3350 3450
Wire Wire Line
	1450 3550 1450 2250
Wire Wire Line
	1450 3550 3350 3550
$Comp
L Connector:Conn_01x10_Male J8
U 1 1 5F2283AD
P 1750 2050
F 0 "J8" V 1585 1978 50  0000 C CNN
F 1 "AFE" V 1676 1978 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x10_P2.54mm_Vertical" H 1750 2050 50  0001 C CNN
F 3 "~" H 1750 2050 50  0001 C CNN
	1    1750 2050
	0    1    1    0   
$EndComp
Wire Wire Line
	1350 2250 1350 3650
Wire Wire Line
	1350 3650 3350 3650
Wire Wire Line
	1250 3750 1250 2250
Wire Wire Line
	1250 3750 3350 3750
Wire Wire Line
	3500 4000 3500 5150
Wire Wire Line
	3500 5150 5150 5150
Wire Wire Line
	5250 2600 5250 1900
Wire Wire Line
	5250 2600 6750 2600
Wire Wire Line
	5350 1900 5350 2700
Wire Wire Line
	5150 3300 6750 3300
Wire Wire Line
	5150 1900 5150 3300
Wire Wire Line
	5050 3200 6750 3200
Wire Wire Line
	5050 1900 5050 3200
Wire Wire Line
	4950 1900 4950 3000
Wire Wire Line
	4950 3000 6750 3000
Wire Wire Line
	6750 2900 4850 2900
Wire Wire Line
	4850 2900 4850 1900
Wire Wire Line
	7600 3400 7600 5200
Wire Wire Line
	7250 3700 7450 3700
Wire Wire Line
	7250 4000 7450 4000
NoConn ~ 7450 3700
NoConn ~ 7450 4000
$EndSCHEMATC

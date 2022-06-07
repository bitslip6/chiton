from thorpy.comm.discovery import discover_stages
import requests
import time
import sys

def zero_angle(stage, angle):
    print("homing...")
    stage.home()
    print("waiting 2.5 seconds (0 angle)" + str(angle) + " ...")
    time.sleep(2.5)
    stage.acceleration = 20.0
    stage.max_velocity = 10.0
    time.sleep(0.5)
    stage.position = angle
    print("waiting 20 seconds to 0...")
    time.sleep(20.0)
    # time.sleep(15.0)
    print("positioned")


def sample_rotations(stage, base, direction, direction2, root_angle):
    radian = 3.141
    radian2 = 3.141
    angle = root_angle
    # 2048
    base = -root_angle * (0.3555 /2)
    root = base
    physical_dist = sys.argv[1]



    # samples = 2248
    # samples = 2400
    samples = 3200
    min_angle = 3.141
    #max_angle = 4.0
    max_angle = 5.54
    inter = (max_angle - min_angle) / samples

    stage.acceleration = 1.00 
    stage.max_velocity = 0.25 
    time.sleep(1.0)
    rad_mul = 180/3.141

    if (direction > 0):
        fin_rad = min_angle + (inter * (samples - 1))
    else:
        fin_rad = min_angle - (inter * (samples - 1))

    time.sleep(1.0)
    print("cur accel: " + str(stage.acceleration))
    print("cur velocity: " + str(stage.velocity))
    print("cur pos: " + str(stage.position))
    print("fin rad: " + str(fin_rad))
    print("fin pos: " + str((fin_rad * rad_mul) -180))

    radian = float(stage.position - 180) / rad_mul
    r = requests.get("http://chiton.lan/sample2/calibrate/"+str(physical_dist)+"/"+str(angle)+"/"+str(radian*10000))

    stage.position = (fin_rad * rad_mul) - 180
    done = False
    last_pos = 999.0
    stagnant_ctr = 0
    i = 1
    while not done:
        # calculate sensor radian angle from stage position
        radian = float(stage.position - 180) / rad_mul
        # display status
        print(str(i) + " pos: " + str(stage.position) + " rad: " + str(radian))
        # sample the angle
        r = requests.get("http://chiton.lan/sample2/calibrate/"+str(physical_dist)+"/"+str(angle)+"/"+str(radian*10000))
        # wait a moment...
        time.sleep(0.033)

        # increment sample counter
        i += 1
        # if stage did not rotate, increase stagnant counter
        if (last_pos == radian):
            stagnant_ctr += 1
        else:
            stagnant_ctr = 0

        # if stage has been stagnant for 10 samples, finish
        if stagnant_ctr == 10:
            done = True

        last_pos = radian

    print("complete")
    time.sleep(10.0)

    return

    # todo: align rotation with sample line
    for i in range(samples):
        if (direction > 0):
            radian += inter
            radian2 -= inter
        else:
            radian -= inter
            radian2 += inter

        angle = radian * rad_mul
        root = angle - 180
        print("angle: " + str(angle) + " root: " + str(root) + " radian: " + str(radian))
        stage.position = root

        if (i % 512) == 511:
            zero_angle(stage, 0)
            print("repositioning")
            stage.position = root
            time.sleep(30.0)

        time.sleep(1.0)
        r = requests.get("http://chiton.lan/sample2/calibrate/"+str(physical_dist)+"/"+str(angle)+"/"+str(radian*2048))

    return



if __name__ == '__main__':
    from thorpy.message import *
    
    # 360 index = 360 * 0.366666
    # root = 365 * 0.355 # 132 # 0 #(360/773)*360
    base = 0 
    angle = 1 
    physical_dist = sys.argv[1]

    
    if physical_dist == "finish":
        print("update active calibration")
        r = requests.get("http://chiton.lan/process/calibrate")
        print("infill missing calibration data...")
        r = requests.get("http://chiton.lan/infill/calibrate")
        r = requests.get("http://chiton.lan/infill/calibrate")
        r = requests.get("http://chiton.lan/infill/calibrate")
        print("update active calibration")
        r = requests.get("http://chiton.lan/process/calibrate")
        print("saving calibration data")
        r = requests.get("http://chiton.lan/save/calibrate")
        print("Calibration saved to sensor at /tmp/calibration.bin")
        sys.exit()


    root = base
    stages = list(discover_stages())
    print(stages)
    stage = stages[0]

    # set rotation stage to a set angle 
    if physical_dist == "rotate":
        stage.acceleration = 20.0
        stage.max_velocity = 8.0
        print("cur min velocity: " + str(stage.min_velocity))
        print("cur max velocity: " + str(stage.max_velocity))
        print("cur acceleration: " + str(stage.acceleration))
        print("rotate to angle: " + str(sys.argv[2]))
        stage.position = float(sys.argv[2])
        sys.exit(0)



    zero_angle(stage, base)

    # disable chiton calibration
    print("disable chiton calibration")
    r = requests.get("http://chiton.lan/chitoncal/setting/off")
    print("enabling smoothing")
    r = requests.get("http://chiton.lan/smoothing/setting/on")
    time.sleep(1.0)

    sample_rotations(stage, base, 1, -1, 0)
    zero_angle(stage, base)

    # base = 116
    # 2048
    sample_rotations(stage, base, -1, -1, 0)
    #sample_rotations(stage, -(773*2), -1, -1, (773*2)-1)



    # r = requests.get("http://chiton.lan/infill/calibrate/")
    # print("infill calibration")
    r = requests.get("http://chiton.lan/save/calibrate/")
    print("saved, zeroing...")
    zero_angle(stage, base)
    print("complete")
    sys.exit(0)



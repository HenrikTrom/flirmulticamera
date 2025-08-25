import pyflircam
import cv2

fcam = pyflircam.FlirMultiCameraHandler("/home/docker/workspace/build/dependencies/FlirMultiCamera/cfg/1024x768.json")
fcam.start()
imgs = fcam.get_images()
fcam.stop()

fp = ""
for idx, img in enumerate(imgs):
    fp = f"test{idx}.jpg"
    cv2.imwrite(fp, img)
    print(f"Saved: {fp}")
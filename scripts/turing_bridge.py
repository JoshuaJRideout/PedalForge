import os
import time
import sys

try:
    from PIL import Image
    from turing_smart_screen_python.lcd.lcd_comm_rev_a import LcdCommRevA # Adjust revision based on hardware
except ImportError:
    print("Please install requirements:")
    print("pip install Pillow pyserial")
    print("git clone https://github.com/mathoudebine/turing-smart-screen-python")
    print("And ensure the library is in your PYTHONPATH.")
    sys.exit(1)

def main():
    # Attempt to find the display COM port automatically
    # This might require configuration on Windows (e.g. COM3) or macOS/Linux (e.g. /dev/ttyUSB0)
    port = "/dev/ttyUSB0" if sys.platform.startswith("linux") else "COM3"
    if sys.platform == "darwin":
        import glob
        ports = glob.glob('/dev/tty.usbserial*') + glob.glob('/dev/cu.usbserial*')
        if ports:
            port = ports[0]
            
    try:
        display = LcdCommRevA(port)
        display.Reset()
        display.clear_surface()
        display.set_brightness(100)
    except Exception as e:
        print(f"Failed to connect to Turing display on {port}: {e}")
        sys.exit(1)

    print(f"Connected to Turing Smart Screen on {port}.")
    print("Waiting for PedalForge images...")

    import tempfile
    img_path = os.path.join(tempfile.gettempdir(), "pedalforge_turing.png")
    
    last_mtime = 0
    
    while True:
        try:
            if os.path.exists(img_path):
                mtime = os.path.getmtime(img_path)
                if mtime != last_mtime:
                    last_mtime = mtime
                    
                    # Image has updated, read it and push it
                    try:
                        img = Image.open(img_path)
                        # We may need to resize or rotate the image depending on the exact screen hardware
                        # The LcdComm library requires drawing the image to a surface or sending it directly
                        display.display_image(img, 0, 0)
                    except Exception as e:
                        # Sometimes we read while the plugin is actively writing to it
                        pass
        except KeyboardInterrupt:
            print("Exiting...")
            break
        except Exception as e:
            print(f"Error: {e}")
            
        time.sleep(0.05) # ~20 fps polling

if __name__ == '__main__':
    main()

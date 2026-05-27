#include "TuringDisplay.h"

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <errno.h>
#include <cstring>

//==============================================================================
TuringDisplay::TuringDisplay() : juce::Thread ("TuringDisplay") {}

TuringDisplay::~TuringDisplay()
{
    stopConnection();
}

//==============================================================================
bool TuringDisplay::startConnection()
{
    const auto path = findDevicePath();
    if (path.isEmpty()) return false;
    if (! openPort (path)) return false;

    sendHandshake();
    setHardwareOrientation();
    startThread (juce::Thread::Priority::low);
    return true;
}

void TuringDisplay::stopConnection()
{
    stopThread (2000);
    closePort();
}

//==============================================================================
void TuringDisplay::pushFrame (const juce::Image& frame)
{
    const juce::ScopedLock sl (bufferLock);
    pendingFrame = frame.createCopy();   // detach so the buffer can be reused
    frameAvailable.store (true);
}

void TuringDisplay::run()
{
    while (!threadShouldExit())
    {
        if (!isConnected())
        {
            // Attempt reconnect every ~1 s while disconnected.
            wait (1000);
            if (threadShouldExit()) return;
            const auto path = findDevicePath();
            if (path.isNotEmpty() && openPort (path))
            {
                sendHandshake();
                setHardwareOrientation();
            }
            continue;
        }

        if (frameAvailable.exchange (false))
        {
            juce::Image frame;
            {
                const juce::ScopedLock sl (bufferLock);
                frame = pendingFrame;
            }
            if (frame.isValid())
            {
                if (!sendImage (frame))
                {
                    // Send failed → assume disconnect, drop the port.
                    DBG ("[Turing] sendImage failed; closing port");
                    closePort();
                    continue;
                }
                // Protocol says 50 ms cooldown after a bitmap blit; we use
                // a generous 200 ms to make sure the device fully commits
                // the frame before the next DISPLAY_BITMAP arrives.
                wait (200);
                continue;
            }
        }

        wait (15);
    }
}

//==============================================================================
// POSIX serial setup — macOS / Linux. JUCE doesn't ship a serial wrapper
// so we go direct.
bool TuringDisplay::openPort (const juce::String& devicePath)
{
    closePort();

    int fd = ::open (devicePath.toRawUTF8(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
        return false;

    // Switch back to blocking for normal writes/reads.
    int flags = fcntl (fd, F_GETFL, 0);
    fcntl (fd, F_SETFL, flags & ~O_NONBLOCK);

    struct termios tio;
    std::memset (&tio, 0, sizeof (tio));
    if (tcgetattr (fd, &tio) != 0)
    {
        ::close (fd);
        return false;
    }

    cfmakeraw (&tio);
    cfsetispeed (&tio, B115200);
    cfsetospeed (&tio, B115200);

    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag |= CRTSCTS;          // hardware flow control per protocol
    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;

    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 10;            // 1.0 s read timeout

    if (tcsetattr (fd, TCSANOW, &tio) != 0)
    {
        ::close (fd);
        return false;
    }

    tcflush (fd, TCIOFLUSH);
    serialFd.store (fd);
    DBG ("[Turing] Opened " << devicePath << " fd=" << fd);
    return true;
}

void TuringDisplay::closePort()
{
    int fd = serialFd.exchange (-1);
    if (fd >= 0) ::close (fd);
}

//==============================================================================
bool TuringDisplay::writeCommand (juce::uint8 cmd, const juce::uint8 payload[8])
{
    juce::uint8 frame[10];
    frame[0] = cmd;
    std::memcpy (frame + 1, payload, 8);
    frame[9] = cmd;     // terminator = command-id repeated
    return writeBytes (frame, 10);
}

bool TuringDisplay::writeBytes (const void* data, size_t length)
{
    const int fd = serialFd.load();
    if (fd < 0) return false;

    const juce::uint8* p = static_cast<const juce::uint8*> (data);
    size_t remaining = length;
    while (remaining > 0)
    {
        ssize_t n = ::write (fd, p, remaining);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        p += n;
        remaining -= static_cast<size_t> (n);
    }
    // Block until the kernel has handed all bytes to the device. Without
    // this, the next writeCommand/writeBytes can race ahead and the device
    // sees pixel bytes appended to the previous frame instead of starting
    // fresh — the "crawling offset" failure mode.
    tcdrain (fd);
    return true;
}

bool TuringDisplay::sendHandshake()
{
    juce::uint8 payload[8] = { 'H', 'E', 'L', 'L', 'O', 0, 0, 0 };
    if (!writeCommand (CMD_HELLO, payload)) return false;

    // Drain any response (sub-revision byte). We don't act on it yet —
    // the protocol implementation here is the A11/A12-style behaviour.
    const int fd = serialFd.load();
    if (fd >= 0)
    {
        juce::uint8 resp[10] {};
        (void) ::read (fd, resp, sizeof (resp));
    }
    return true;
}

bool TuringDisplay::setHardwareOrientation()
{
    // We render in the device's native portrait (320×480) orientation
    // and apply any user-set rotation in software in applyRotation().
    juce::uint8 payload[8] = { ORIENTATION_PORTRAIT, 0, 0, 0, 0, 0, 0, 0 };
    return writeCommand (CMD_SET_ORIENTATION, payload);
}

//==============================================================================
juce::Image TuringDisplay::applyRotation (const juce::Image& src) const
{
    const int angle = (int) rotation;
    if (angle == 0) return src;

    juce::AffineTransform t;
    if (angle == 90)
        t = juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi)
            .translated ((float) src.getHeight(), 0.0f);
    else if (angle == 180)
        t = juce::AffineTransform::rotation (juce::MathConstants<float>::pi)
            .translated ((float) src.getWidth(), (float) src.getHeight());
    else if (angle == 270)
        t = juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
            .translated (0.0f, (float) src.getWidth());

    const int outW = (angle == 90 || angle == 270) ? src.getHeight() : src.getWidth();
    const int outH = (angle == 90 || angle == 270) ? src.getWidth()  : src.getHeight();
    juce::Image rotated (juce::Image::RGB, outW, outH, true);
    juce::Graphics g (rotated);
    g.drawImageTransformed (src, t);
    return rotated;
}

//==============================================================================
bool TuringDisplay::sendImage (const juce::Image& src)
{
    // Re-assert orientation every frame in case the device's state gets
    // reset by the previous bitmap or by an unrelated interrupt.
    setHardwareOrientation();

    // Bring whatever was rendered down to the panel's native 480x320 in
    // landscape, applying user rotation along the way.
    juce::Image rotated = applyRotation (src);

    juce::Image canvas (juce::Image::RGB, kNativeWidth, kNativeHeight, true);
    {
        juce::Graphics g (canvas);
        g.fillAll (juce::Colours::black);
        const float sx = (float) kNativeWidth  / (float) rotated.getWidth();
        const float sy = (float) kNativeHeight / (float) rotated.getHeight();
        const float s  = juce::jmin (sx, sy);
        const float w  = rotated.getWidth()  * s;
        const float h  = rotated.getHeight() * s;
        g.drawImageTransformed (rotated,
            juce::AffineTransform::scale (s, s).translated ((kNativeWidth - w) * 0.5f,
                                                            (kNativeHeight - h) * 0.5f));
    }

    // DISPLAY_BITMAP header — full-screen rectangle 0,0 → 479,319
    constexpr int x0 = 0, y0 = 0;
    constexpr int x1 = kNativeWidth - 1;
    constexpr int y1 = kNativeHeight - 1;
    juce::uint8 payload[8] = {
        (juce::uint8) ((x0 >> 8) & 0xFF), (juce::uint8) (x0 & 0xFF),
        (juce::uint8) ((y0 >> 8) & 0xFF), (juce::uint8) (y0 & 0xFF),
        (juce::uint8) ((x1 >> 8) & 0xFF), (juce::uint8) (x1 & 0xFF),
        (juce::uint8) ((y1 >> 8) & 0xFF), (juce::uint8) (y1 & 0xFF)
    };
    if (!writeCommand (CMD_DISPLAY_BITMAP, payload))
        return false;

    // Encode RGB565 big-endian, row-major. 480*320*2 = 307200 bytes.
    // Use getPixelColour() so we don't depend on JUCE's platform-specific
    // BGR/RGB byte order for Image::RGB raw pixels.
    const int pixelCount = kNativeWidth * kNativeHeight;
    std::vector<juce::uint8> rgb565;
    rgb565.resize ((size_t) pixelCount * 2);

    juce::Image::BitmapData bd (canvas, juce::Image::BitmapData::readOnly);
    size_t out = 0;
    for (int y = 0; y < kNativeHeight; ++y)
    {
        for (int x = 0; x < kNativeWidth; ++x)
        {
            const auto c = bd.getPixelColour (x, y);
            const juce::uint8 r = c.getRed();
            const juce::uint8 g = c.getGreen();
            const juce::uint8 b = c.getBlue();
            const juce::uint16 pixel = (juce::uint16)
                (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
            // Little-endian on the wire. Anti-aliased text comes out as
            // green/magenta camo with big-endian here, suggesting this
            // Rev B variant expects low byte first.
            rgb565[out++] = (juce::uint8) (pixel & 0xFF);
            rgb565[out++] = (juce::uint8) ((pixel >> 8) & 0xFF);
        }
    }

    // Match the Python reference: chunked(rgb565be, width * 8) — that's
    // width × 8 BYTES per chunk. For 320 wide this is 2560 bytes per
    // write. writeBytes() calls tcdrain() so each chunk is fully delivered
    // before the next ships, matching pySerial's flush()-on-Darwin
    // behaviour. We also wait a few ms between chunks because macOS
    // CH340 driver instances sometimes drop bytes under sustained writes
    // even with RTS/CTS — symptom is a horizontal offset that oscillates
    // frame to frame.
    const size_t chunkSize = (size_t) kNativeWidth * 8;
    size_t sent = 0;
    while (sent < rgb565.size())
    {
        const size_t take = std::min (chunkSize, rgb565.size() - sent);
        if (!writeBytes (rgb565.data() + sent, take)) return false;
        sent += take;
        if (sent < rgb565.size())
            wait (2);   // ~5 ms breather; full frame still <600 ms
    }
    return true;
}

//==============================================================================
juce::String TuringDisplay::findDevicePath()
{
    // /dev/cu.* entries are character devices, not regular files, so
    // juce::File::findChildFiles filters them out. Scan via POSIX
    // opendir/readdir instead.
    DIR* dir = ::opendir ("/dev");
    if (dir == nullptr) return {};

    juce::String result;
    while (auto* ent = ::readdir (dir))
    {
        const juce::String name (ent->d_name);
        if (! name.startsWith ("cu.usbmodem")) continue;
        // The Turing 3.5" V2 reports a device path that contains its
        // hard-coded serial "USB35INCHIPSV2".
        if (name.containsIgnoreCase ("USB35INCHIPSV2"))
        {
            result = "/dev/" + name;
            break;
        }
    }
    ::closedir (dir);
    return result;
}

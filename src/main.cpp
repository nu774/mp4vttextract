#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include "libplatform/endian.h"
#include "src/impl.h"

struct vtt_cue_t {
    MP4Timestamp pts;
    MP4Duration duration;
    std::string iden;
    std::string sttg;
    std::string payl;

    vtt_cue_t(): pts(0), duration(0) {}
};

#define FOURCC(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))

uint32_t readNextBox(const uint8_t *p, const uint8_t *last, uint32_t *size)
{
    uint32_t name = 0;
    if (p < last - 8) {
        std::memcpy(size, p, 4);
        std::memcpy(&name, p + 4, 4);
        *size = MP4V2_HTONL(*size);
        name = MP4V2_HTONL(name);
    }
    return name;
}

vtt_cue_t parseVTTBoxes(const uint8_t *data, const uint8_t *end)
{
    uint32_t name, boxSize;
    vtt_cue_t cue;
    while ((name = readNextBox(data, end, &boxSize)) != 0) {
        switch (name) {
        case FOURCC('i','d','e','n'):
            cue.iden = std::string(data + 8, data + boxSize);
            break;
        case FOURCC('s','t','t','g'):
            cue.sttg = std::string(data + 8, data + boxSize);
            break;
        case FOURCC('p','a','y','l'):
            cue.payl = std::string(data + 8, data + boxSize);
            break;
        }
        data += boxSize;
    }
    return cue;
}

std::vector<vtt_cue_t> parseVTTSample(const uint8_t *data, uint32_t size)
{
    std::vector<vtt_cue_t> res;
    uint32_t name, boxSize;
    const uint8_t *end = data + size;
    while ((name = readNextBox(data, end, &boxSize)) != 0) {
        if (name == FOURCC('v','t','t','c'))
            res.push_back(parseVTTBoxes(data + 8, data + boxSize));
        data += boxSize;
    }
    return res;
}

bool isVTTTextTrack(mp4v2::impl::MP4File &file, int tid)
{
    mp4v2::impl::MP4Track *track = file.GetTrack(tid);
    const char *hdlr = track->GetType();
    if (std::strcmp(hdlr, "text"))
        return false;
    mp4v2::impl::MP4Atom &trakAtom = track->GetTrakAtom();
    mp4v2::impl::MP4Atom *stsd = trakAtom.FindChildAtom("mdia.minf.stbl.stsd");
    if (!stsd)
        return false;
    mp4v2::impl::MP4Atom *descAtom = stsd->GetChildAtom(0);
    const char *descName = descAtom->GetType();
    return !std::strcmp(descName, "svtt") || !std::strcmp(descName, "wvtt");
}

int findVTTTextTrack(mp4v2::impl::MP4File &file, int desiredTrack)
{
    int ntracks = file.GetNumberOfTracks();
    if (desiredTrack > 0) {
        return isVTTTextTrack(file, desiredTrack) ? desiredTrack : 0;
    }
    for (int tid = 1; tid <= ntracks; ++tid)
        if (isVTTTextTrack(file, tid))
            return tid;
    return 0;
}

std::string formatTime(double seconds)
{
    int h = seconds / 3600;
    seconds -= h * 3600;
    int m = seconds / 60;
    seconds -= m * 60;
    int s = seconds;
    int millis = (seconds - s) * 1000;
    std::stringstream ss;
    ss << h
       << ':'
       << std::setfill('0') << std::right << std::setw(2) << m
       << ':'
       << std::setfill('0') << std::right << std::setw(2) << s
       << '.'
       << std::setfill('0') << std::right << std::setw(3) << millis;
    return ss.str();
}

void putCue(const vtt_cue_t &cue, uint32_t timescale)
{
    double from = static_cast<double>(cue.pts) / timescale;
    double to = static_cast<double>(cue.pts + cue.duration) / timescale;
    if (!cue.iden.empty())
        puts(cue.iden.c_str());
    std::string sfrom = formatTime(from);
    std::string sto = formatTime(to);
    std::stringstream ss;
    ss << sfrom << " --> " << sto;
    if (!cue.sttg.empty())
        ss << " " << cue.sttg;
    std::puts(ss.str().c_str());
    std::printf("%s\n\n", cue.payl.c_str());
}

void extract(mp4v2::impl::MP4Track *track)
{
    uint32_t nsamples = track->GetNumberOfSamples();
    uint32_t timescale = track->GetTimeScale();
    std::vector<vtt_cue_t> prev_cues;

    std::printf("WEBVTT\n\n");

    for (uint32_t i = 1; i <= nsamples; ++i) {
        uint8_t *data = nullptr;
        uint32_t size = 0;
        MP4Timestamp pts;
        MP4Duration duration;
        track->ReadSample(i, &data, &size, &pts, &duration);
        auto cues = parseVTTSample(data, size);
        for (auto &&cue: cues) {
            cue.pts = pts;
            cue.duration = duration;
        }
        MP4Free(data);
        auto i1 = std::begin(prev_cues);
        while (i1 != std::end(prev_cues)) {
            bool same_id = false;
            auto i2 = std::begin(cues);
            while (i2 != std::end(cues)) {
                if (i1->iden != i2->iden)
                    ++i2;
                else {
                    same_id = true;
                    i1->duration += i2->duration;
                    i2 = cues.erase(i2);
                }
            }
            if (same_id)
                ++i1;
            else {
                putCue(*i1, timescale);
                i1 = prev_cues.erase(i1);
            }
        }
        for (auto &&cue: cues) {
            if (cue.iden.empty())
                putCue(cue, timescale);
            else
                prev_cues.push_back(cue);
        }
    }
    for (auto &&cue: prev_cues)
        putCue(cue, timescale);
}

void execute(const char *infile, int track)
{
    try {
        mp4v2::impl::log.setVerbosity(MP4_LOG_NONE);
        mp4v2::impl::MP4File file;
        file.Read(infile, 0);
        int tid = findVTTTextTrack(file, track);
        if (tid == 0) {
            throw std::runtime_error("VTT text track is not found");
        }
        extract(file.GetTrack(tid));
    } catch (mp4v2::impl::Exception *e) {
        std::stringstream ss;
        ss << "libmp4v2: " << e->msg().c_str();
        std::runtime_error re(ss.str());
        delete e;
        throw re;
    }
}

void usage()
{
    std::fprintf(stderr, "usage: mp4vttextract [-t TRACK] IN.mp4 [OUT.vtt]\n");
}

int main(int argc, char **argv)
{
    try {
        int c;
        int track = 0;
        while ((c = getopt(argc, argv, "t:")) != -1) {
            switch (c) {
            case 't':
                track = atoi(optarg);
                break;
            default:
                usage();
                return 1;
            }
        }
        argc -= optind;
        argv += optind;
        if (argc == 0) {
            usage();
            return 1;
        }
        if (argv[1])
            std::freopen(argv[1], "w", stdout);
        execute(argv[0], track);
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 2;
    }
}

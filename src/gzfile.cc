#include "gzfile.h"

#include <cstring>

void GzFile::reset()
{
    if (is_deflate_) {
      is_deflate_ = false;
        inflateEnd(&zbuf_);
    }
    pos_ = skip_ = start_ = 0;
    memset(&zbuf_, 0, sizeof zbuf_);
}

void GzFile::close()
{
    reset();
}

bool GzFile::open()
{
    reset();

    // save start of file to rewind for seeking
    start_ = file_->ftell();

    int64_t count = file_->fread(buffer_.data(), 1, 4096);

    if (count < 0)
        // FIXME: General error handling and logging?
        return false;

    zbuf_.avail_in = count;
    zbuf_.next_in = buffer_.data();

    if (mode_ == MODE_AUTO) {
        const int min_gzip_size = 2 + 8 + 8;
        if (count >= min_gzip_size && buffer_[0] == 0x1F &&
            buffer_[1] == 0x8B) { // gzip header
          mode_ = MODE_UNGZ;
        }
    }

    int wb = MAX_WBITS + 16;
    if (mode_ == MODE_RAW_DEFLATE) {
        wb = -MAX_WBITS;
    }

    int zerr = inflateInit2(&zbuf_, wb);
    if (zerr)
        return false;
    is_deflate_ = true;

    return true;
}

int64_t GzFile::read(void *buffer, uint64_t n_bytes)
{
    if (!n_bytes)
        return 0;

    zbuf_.next_out = static_cast<Bytef*>(buffer);
    zbuf_.avail_out = n_bytes;

    while (true) {
        unsigned old_avail_in = zbuf_.avail_in;
        int err = inflate(&zbuf_, Z_NO_FLUSH);

        if (err == Z_STREAM_END) {
            n_bytes -= zbuf_.avail_out;
            reset();
            break;
        }

        if (err == Z_BUF_ERROR && !old_avail_in) {
            err = 0; // need more data
        }

        if (err)
            return 0;

        if (!zbuf_.avail_out)
            break;

        if (zbuf_.avail_in) {
            return 0;
        }

        unsigned count = file_->fread(buffer_.data(), 1, buffer_.size());
        zbuf_.next_in = buffer_.data();
        zbuf_.avail_in = count;
        if (!zbuf_.avail_in)
            return 0;
    }
    return n_bytes;
}

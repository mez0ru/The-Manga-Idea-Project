#include "JpegXLExif.h"

dimensionsInfo GetBasicInfo(const uint8_t* jxl, size_t size) {

    auto dec = JxlDecoderMake(nullptr);

    if (!dec) {
        return dimensionsInfo{ 0, 0, false };
    }

    if (JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO)) {
        return dimensionsInfo{ 0, 0, false };
    }

    JxlDecoderSetInput(dec.get(), jxl, size);
    JxlDecoderCloseInput(dec.get());

    JxlBasicInfo info;

    for (;;) {
        // The first time, this will output JXL_DEC_NEED_MORE_INPUT because no
        // input is set yet, this is ok since the input is set when handling this
        // event.
        JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());

        if (status == JXL_DEC_ERROR) {
            //fprintf(stderr, "Decoder error\n");
            break;
        }
        else if (status == JXL_DEC_NEED_MORE_INPUT) {
            break;
        }
        else if (status == JXL_DEC_SUCCESS) {
            // Finished all processing.
            break;
        }
        else if (status == JXL_DEC_BASIC_INFO) {
            if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec.get(), &info)) {

                break;
            }
            return { info.xsize, info.ysize, true };
        }
        else {
            break;
        }
    }

    JxlDecoderDestroy(dec.get());

    return dimensionsInfo{ 0, 0, false };
}
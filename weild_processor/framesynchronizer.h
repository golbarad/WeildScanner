#ifndef FRAMESYNCHRONIZER_H
#define FRAMESYNCHRONIZER_H

#include "cvmetatypes.h"

class FrameSynchronizer
{
public:

    struct DualFrame {
        cv::Mat left_frame;
        cv::Mat right_frame;

        bool empty() const { return left_frame.empty() && right_frame.empty(); }
    };

    FrameSynchronizer();
};

Q_DECLARE_METATYPE(FrameSynchronizer::DualFrame)

#endif // FRAMESYNCHRONIZER_H

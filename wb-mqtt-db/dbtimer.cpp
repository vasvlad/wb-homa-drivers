#include "dblogger.h"

#include <chrono>
#include <algorithm>
#include <logging.h>

using namespace std;
using namespace std::chrono;

std::ostream& operator<<(std::ostream& out, const struct TChannelName &name){
    out << name.Device << "/" << name.Control;
    return out;
}

steady_clock::time_point TMQTTDBLogger::ProcessTimer(steady_clock::time_point next_call)
{
    auto now = steady_clock::now();
    bool start_process = false;

    if (next_call > now) {
        return next_call; // there is some time to wait
    }

#ifndef NDEBUG
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
#endif

    SQLite::Transaction transaction(*DB);

    // for each group - check processing time according to its configuration
 
    for (auto& group : LoggerConfig.Groups) {
   
        bool process_changed = true;
        bool group_processed = false;

        for (auto channel_id : group.ChannelIds) {

            auto& channel_data = ChannelDataCache[channel_id];

            // check if current group is ready to process changed values
            // or ready to process unchanged values
            if ((now >= group.LastSaved + seconds(group.MinInterval) && channel_data.Changed) || 
                (now >= group.LastUSaved + seconds(group.MinUnchangedInterval) &&
                 !channel_data.Changed && channel_data.LastProcessed >= group.LastUSaved)) {

                if (!start_process) {
                    start_process = true;
                    next_call = now + seconds(min(group.MinInterval, group.MinUnchangedInterval));

                    LOG(INFO) << "Start bulk transaction";
                }

                LOG(INFO) << "Processing channel " << channel_data.Name << " from group " << group.Id << (channel_data.Changed ? ", changed" : ", UNCHANGED");
                DLOG(DEBUG) << "Selected channel ID is " << channel_id;

                process_changed = process_changed && channel_data.Changed;
                group_processed = true;

                WriteChannel(channel_data, group);
            }

        }

        if (group_processed) {
            group.LastSaved = now;
            if (!process_changed) {
                LOG(INFO) << "Unchanged values processed!";
                group.LastUSaved = now;
            }
        }

        // select minimal next call time
        if (next_call > now + seconds(min(group.MinInterval, group.MinUnchangedInterval))) {
            next_call = now + seconds(min(group.MinInterval, group.MinUnchangedInterval));
        }

    }

    if (start_process)
        transaction.commit();
 
#ifndef NDEBUG
    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();

    if (start_process)
        DLOG(INFO) << "Bulk processing took " << duration << "ms";
#endif

    return next_call;
}

void TMQTTDBLogger::WriteChannel(TChannel &channel_data, TLoggingGroup &group)
{
    int channel_int_id, device_int_id;

    tie(channel_int_id, device_int_id) = GetOrCreateIds(channel_data.Name);

    DLOG(DEBUG) << "Resulting channel ID for this request is " << channel_int_id;

    static SQLite::Statement insert_row_query(*DB, "INSERT INTO data (device, channel, value, group_id, min, max, \
                                                    retained) VALUES (?, ?, ?, ?, ?, ?, ?)");

    insert_row_query.reset();
    insert_row_query.bind(1, device_int_id);
    insert_row_query.bind(2, channel_int_id);
    insert_row_query.bind(4, group.IntId);

    // min, max and average
    if (channel_data.Accumulated && channel_data.Changed) {
        auto& accum = channel_data.Accumulator;
        double val = accum.ValueCount > 0 ? accum.Sum / accum.ValueCount : 0.0; // 0.0 - error value

        insert_row_query.bind(3, val); // avg

        insert_row_query.bind(5, accum.Min); // min
        insert_row_query.bind(6, accum.Max); // max

        accum.Reset();
    } else {
        insert_row_query.bind(3, channel_data.LastValue); // avg == value
        insert_row_query.bind(5); // bind NULL values
        insert_row_query.bind(6); // bind NULL values
    }

    channel_data.Changed = false;

    insert_row_query.bind(7, channel_data.Retained ? 1 : 0);

    insert_row_query.exec();


    DLOG(DEBUG) << channel_data.Name << ": " << insert_row_query.getQuery();

    // local cache is needed here since SELECT COUNT are extremely slow in sqlite
    // so we only ask DB at startup. This applies to two if blocks below.

    if (group.Values > 0) {
        if ((++channel_data.RowCount) > group.Values * (1 + RingBufferClearThreshold) ) {
            static SQLite::Statement clean_channel_query(*DB, "DELETE FROM data WHERE channel = ? ORDER BY rowid ASC LIMIT ?");
            clean_channel_query.reset();
            clean_channel_query.bind(1, channel_int_id);
            clean_channel_query.bind(2, ChannelDataCache[channel_int_id].RowCount - group.Values);

            clean_channel_query.exec();

            SYSLOG(WARN) << "Channel data limit is reached: channel " << channel_data.Name 
                    << ", row count " << channel_data.RowCount << ", limit " << group.Values;

            DLOG(DEBUG) << clean_channel_query.getQuery();

            channel_data.RowCount = group.Values;
        }
    }

    if (group.ValuesTotal > 0) {
        if ((++GroupRowNumberCache[group.IntId]) > group.ValuesTotal * (1 + RingBufferClearThreshold)) {
            static SQLite::Statement clean_group_query(*DB, "DELETE FROM data WHERE group_id = ? ORDER BY rowid ASC LIMIT ?");
            clean_group_query.reset();
            clean_group_query.bind(1, group.IntId);
            clean_group_query.bind(2, GroupRowNumberCache[group.IntId] - group.ValuesTotal);
            clean_group_query.exec();

            SYSLOG(WARN) << "Group data limit is reached: group " << group.Id
                    << ", row count " << GroupRowNumberCache[group.IntId] << ", limit " << group.ValuesTotal;

            DLOG(DEBUG) << clean_group_query.getQuery();

            GroupRowNumberCache[group.IntId] = group.ValuesTotal;
        }
    }
}

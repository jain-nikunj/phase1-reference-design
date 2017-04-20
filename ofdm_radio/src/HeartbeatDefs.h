/* HeartbeatDefs.h
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#ifndef HEARTBEATDEFS_H_
#define HEARTBEATDEFS_H_

// The "heartbeat" signal is the basis of determining if the radio net
// is being jammed or not; whether or not the loss of the heartbeat
// triggers some form of adapatation is based on the heartbeat policy

#define P2M_HEARTBEAT_SAMPLE_WINDOW                 3
#define P2M_HEARTBEAT_NOMINAL_BEATS_PER_SAMPLE      2

enum HeartbeatActivityType {
    HEARTBEAT_ACTIVITY_NONE,
    HEARTBEAT_ACTIVITY_PER_SCHEDULE
};

enum HeartbeatPolicyType {
    HEARTBEAT_POLICY_LOCKED_TO_FDD,
    HEARTBEAT_POLICY_LOCKED_TO_FH,
    HEARTBEAT_POLICY_A,
    HEARTBEAT_POLICY_B
};


#endif // HEARTBEATDEFS_H_

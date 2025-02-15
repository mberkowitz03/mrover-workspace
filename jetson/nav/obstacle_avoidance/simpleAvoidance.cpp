#include "utilities.hpp"
#include "stateMachine.hpp"
#include "simpleAvoidance.hpp"

#include <cmath>
#include <utility>

// Constructs a SimpleAvoidance object with the input mStateMachine, rover, and roverConfig.
// SimpleAvoidance is abstacted from ObstacleAvoidanceStateMachine object so it creates an
// ObstacleAvoidanceStateMachine object with the mStateMachine, rover, and roverConfig.
// The SimpleAvoidance object will execute the logic for the simple avoidance algorithm
SimpleAvoidance::SimpleAvoidance(std::weak_ptr<StateMachine> roverStateMachine, std::shared_ptr<Rover> rover, const rapidjson::Document& roverConfig)
        : ObstacleAvoidanceStateMachine(move(roverStateMachine), move(rover), roverConfig) {}

// Destructs the SimpleAvoidance object.
SimpleAvoidance::~SimpleAvoidance() = default;

// Turn away from obstacle until it is no longer detected.
// If in search state and target is both detected and reachable, return NavState TurnToTarget.
// ASSUMPTION: There is no rock that is more than 8 meters (pathWidth * 2) in diameter
NavState SimpleAvoidance::executeTurnAroundObs(std::shared_ptr<Rover> rover, const rapidjson::Document& roverConfig) {
    throw std::runtime_error("implement me");
} // executeTurnAroundObs()

// Drives to dummy waypoint. Once arrived, rover will drive to original waypoint
// ( original waypoint is the waypoint before obstacle avoidance was triggered )
NavState SimpleAvoidance::executeDriveAroundObs(std::shared_ptr<Rover> rover, const rapidjson::Document& roverConfig) {
    throw std::runtime_error("implement me");
} // executeDriveAroundObs()

// Create the odometry point used to drive around an obstacle
Odometry SimpleAvoidance::createAvoidancePoint(std::shared_ptr<Rover> rover, const double distance) {
    Odometry avoidancePoint = rover->odometry();
    double totalLatitudeMinutes = avoidancePoint.latitude_min +
                                  cos(degreeToRadian(avoidancePoint.bearing_deg)) * distance * LAT_METER_IN_MINUTES;
    double totalLongitudeMinutes = avoidancePoint.longitude_min +
                                   sin(degreeToRadian(avoidancePoint.bearing_deg)) * distance * rover->longMeterInMinutes();
    avoidancePoint.latitude_deg += static_cast<int32_t>(totalLatitudeMinutes / 60);
    avoidancePoint.latitude_min = totalLatitudeMinutes - (totalLatitudeMinutes / 60) * 60;
    avoidancePoint.longitude_deg += static_cast<int32_t>(totalLongitudeMinutes / 60);
    avoidancePoint.longitude_min = totalLongitudeMinutes - (totalLongitudeMinutes / 60) * 60;

    return avoidancePoint;

} // createAvoidancePoint()

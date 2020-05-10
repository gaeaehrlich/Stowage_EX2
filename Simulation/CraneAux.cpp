#include "Crane.h"


void Crane::containerNotFoundError(const string &place) {
    std::ofstream file;
    file.open(_error_path, std::ios::out | std::ios::app);
    file << _sail_info << "Trying to get container that's not " << place << ". Instruction terminated.\n";
    file.close();
}

int Crane::shouldReject(unique_ptr<Container>& container, ShipPlan& plan, ShipRoute& route, bool write) {
    int errors = 0;
    if(!_container_data[container -> getId()].empty()) {
        if(write) { writeLoadError(container -> getId(), "There is more than one container with this ID on port.\n"); }
        errors++;
    }
    if(plan.hasContainer(container -> getId())) {
        if(write) { writeLoadError(container -> getId(), "The ship plan already has a container with this ID.\n"); }
        errors++;
    }
    if(container -> getWeight() < 0) {
        if(write) { writeLoadError(container -> getId(), "The container's weight is illegal.\n"); }
        errors++;
    }
    if(!Reader::legalPortSymbol(container -> getDest())) {
        if(write) { writeLoadError(container -> getId(), "The container's destination port symbol is illegal.\n"); }
        errors++;
    }
    if(!Reader::legalContainerId(container -> getId())) {
        if(write) { writeLoadError(container -> getId(), "The container's ID is not in format.\n"); }
        errors++;
    }
    if(container -> getId().empty()) {
        if(write) { writeLoadError(container -> getId(), "The container's ID is illegal.\n"); }
        errors++;
    }
    if(route.isLastStop()) {
        if(write) { writeLoadError(container -> getId(), "The container to load is at the last stop.\n"); }
        errors++;
    }
    if(plan.isFull()) errors++; // this is not a legal load, this is a fatal error
    if(!route.portInRoute(container -> getDest())) {
        if(write) { writeLoadError(container -> getId(), "The container's destination port is not in route.\n"); }
        errors++;
    }
    if(container-> getDest() == _port) {
        if(write) { writeLoadError(container -> getId(), "The container's destination port is this port.\n"); }
        errors++;
    }
    return errors;
}

void Crane::writeLoadError(const string& id, const string& reason) {
    std::ofstream file;
    file.open(_error_path, std::ios::out | std::ios::app); // file gets created if it doesn't exist and appends to the end
    file << _sail_info << "ERROR: Algorithm is loading the container " << id << " that should be rejected. Rejection reason:" << reason;
    file.close();
}


void Crane::writeInstructionError(const string &instruction, const string &id, bool executed) {
    std::ofstream file;
    file.open(_error_path, std::ios::out | std::ios::app); // file gets created if it doesn't exist and appends to the end
    string exec = executed ? "still" : "not";
    file << _sail_info << "ERROR: Algorithm is making a mistake with container " << id << ". " << instruction << " was " << exec << "executed.\n";
    file.close();
}


bool Crane::isErrorLoad(unique_ptr<Container> &container, ShipPlan& plan, ShipRoute& route, Position pos, bool& fatal) {
    Position lowerFloor = Position(pos._floor - 1, pos._x, pos._y);
    bool isLegalLocation = plan.isLegalLocation(pos) && plan.isEmptyPosition(pos);
    bool cellBelowNull = pos._floor > 0 ? (plan.isLegalLocation(lowerFloor) && plan.isEmptyPosition(lowerFloor)) : false;
    if(!isLegalLocation || cellBelowNull) {
        writeInstructionError("Load", container -> getId(), false);
        fatal = true;
        return true;
    }
    if(shouldReject(container, plan, route, true) > 0) {
        writeInstructionError("Load", container -> getId(), true);
        return true;
    }
    return false;
}


bool Crane::isErrorUnload(const string& id, ShipPlan &plan, Position pos, bool& fatal) {
    bool isLegalLocation = plan.isLegalLocation(pos);
    Position aboveFloor = Position(pos._floor + 1, pos._x, pos._y);
    bool cellAboveNull = pos._floor + 1 == plan.numberOfFloors() ? true : plan.isLegalFloor(aboveFloor) && !plan.isEmptyPosition(aboveFloor);

    if(!plan.hasContainer(id) || !isLegalLocation || !cellAboveNull
       || !_container_data[id].empty() || _temporary_unloaded.find(id) != _temporary_unloaded.end()) {
        writeInstructionError("Unload", id, false);
        fatal = true;
        return true;
    }

    if(plan.getIdAtPosition(pos) != id) {
        std::ofstream file;
        file.open(_error_path, std::ios::out | std::ios::app); // file gets created if it doesn't exist and appends to the end
        file << _sail_info << "ERROR: Algorithm is unloading the container " << plan.getIdAtPosition(pos) << " instead of " << id << ".\n";
        file.close();
        writeInstructionError("Unload", id, true);
        return true;
    }
    return false;
}

void Crane::writeLeftAtPortError(const string& id, const string& msg) {
    std::ofstream file;
    file.open(_error_path, std::ios::out | std::ios::app); // file gets created if it doesn't exist and appends to the end
    file << _sail_info << "ERROR: Algorithm is making a mistake with container " << id
         << ". It " << msg << ", and was wrongly left at port " << _port << ".\n";
    file.close();
}

bool Crane::checkWronglyUnloaded(ShipPlan& plan, ShipRoute& route) {
    bool flag = true;
    for(auto& element : _container_data) {
        unique_ptr<Container>& container = element.second[0]; // should be only one container in vector
        if ((container -> getDest() != _port) && (!plan.isFull() || shouldPrioritize(container -> getDest(), route))) {
            writeLeftAtPortError(container -> getId(), "should have been loaded");
            flag = false;
        }
    }
    return flag;
}


bool Crane::shouldPrioritize(const string &dest, ShipRoute& route) {
    for(const string& port: _newly_loaded_dest) {
        if(route.isStopAfter(port, dest)) {
            return false;
        }
    }
    return true;
}

bool Crane::checkLoadedTemporaryUnloaded() {
    bool flag = true;
    if(!_temporary_unloaded.empty()) {
        for(const string& id: _temporary_unloaded) {
            writeLeftAtPortError(id, "was temporary unloaded");
        }
        flag = false;
    }
    return flag;
}

bool Crane::checkShip(ShipPlan &plan) {
    for(int i = 0; i < plan.numberOfFloors(); i++) {
        for(const pair<int, int>& location: plan.getFloor(i).getLegalLocations()) {
            if(plan.getFloor(i).getContainerDest(location) == _port) {
                return false;
            }
        }
    }
    return true;
}

bool Crane::handleLastStop(ShipPlan &plan, ShipRoute &route) {
    return !(route.isLastStop() && !plan.isEmpty());
}
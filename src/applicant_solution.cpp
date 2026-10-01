
//
// Created by dusan on 9/15/26.
//

#include "../include/antworld.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

void AntWorld::forage()
{
    static AntWorld *lastWorld = nullptr;
    static MapTemplate scannedMap;
    static bool printedStuckAnt = false;

    // Remember which ants have no food or pheromone task this step.
    std::vector<int> freeAntIndices;

    if (lastWorld != this ||
        scannedMap.size() != this->foodMap.size() ||
        (scannedMap.empty() == false &&
         scannedMap[0].size() != this->foodMap[0].size()))
    {
        scannedMap = MapTemplate(
            this->foodMap.size(),
            std::vector<int>(this->foodMap[0].size(), 0)
        );

        lastWorld = this;
        printedStuckAnt = false;
    }

    // First pass: deal with food and pheromones before assigning exploration.
    for (int antIndex = 0;
         antIndex < static_cast<int>(this->ants.size());
         antIndex++)
    {
        Ant &ant = this->ants[antIndex];

        // Carrying food is always the highest priority.
        if (ant.carryingFood == true)
        {
            ant.returnHome(this->terrainMap, this->foodMap);
            continue;
        }

        // A marker whose owner is the last ant cannot be answered by anyone.
        if (this->ants.size() == 1 && ant.pheromoneDropped == true)
        {
            ant.erasePheromone(this->pheromoneMap);
        }

        // Keep an almost-exhausted pheromone owner alive until its marker is resolved.
        if (ant.pheromoneDropped == true && ant.energy <= 1)
        {
            continue;
        }

        // Scan before considering a pheromone assignment.
        std::vector<Coord> visibleFood = ant.foodScan(this->foodMap);

        // Record only the cells covered by this ant's food sensor.
        for (int row = ant.position.first - ant.foodRadius;
             row <= ant.position.first + ant.foodRadius;
             row++)
        {
            for (int column = ant.position.second - ant.foodRadius;
                 column <= ant.position.second + ant.foodRadius;
                 column++)
            {
                if (row >= 0 &&
                    row < static_cast<int>(scannedMap.size()) &&
                    column >= 0 &&
                    column < static_cast<int>(scannedMap[0].size()))
                {
                    scannedMap[row][column] = 1;
                }
            }
        }

        // Compare visible food using the full trip: ant -> food -> home.
        if (visibleFood.empty() == false)
        {
            bool foodChosen = false;
            Coord bestFood;
            int bestTotalCost = 0;
            int bestCostToFood = 0;
            std::vector<Coord> bestPathFoodToHome;

            for (Coord food : visibleFood)
            {
                std::vector<Coord> pathToFood =
                    shortestPath(this->terrainMap, ant.position, food);

                std::vector<Coord> pathFoodToHome =
                    shortestPath(this->terrainMap, food, ant.homeCoord);

                // Ignore a destination if either route does not exist.
                if (pathToFood.empty() == true ||
                    pathFoodToHome.empty() == true)
                {
                    continue;
                }

                int costToFood =
                    calculatePathCost(this->terrainMap, pathToFood);

                int costFoodToHome =
                    calculatePathCost(this->terrainMap, pathFoodToHome);

                int totalDeliveryCost = costToFood + costFoodToHome;

                if (foodChosen == false ||
                    totalDeliveryCost < bestTotalCost)
                {
                    bestFood = food;
                    bestTotalCost = totalDeliveryCost;
                    bestCostToFood = costToFood;
                    bestPathFoodToHome = pathFoodToHome;
                    foodChosen = true;
                }
            }

            if (foodChosen == true)
            {
                // Complete delivery if the ant can afford both parts of the trip.
                if (ant.energy >= bestTotalCost)
                {
                    ant.move(
                        this->terrainMap,
                        bestFood,
                        this->foodMap
                    );

                    ant.returnHome(
                        this->terrainMap,
                        this->foodMap
                    );

                    continue;
                }

                // Otherwise, relay only if food can move at least one step home.
                if (ant.energy >= bestCostToFood &&
                    bestPathFoodToHome.size() >= 2)
                {
                    int nextRow = bestPathFoodToHome[1].first;
                    int nextColumn = bestPathFoodToHome[1].second;

                    int foodRow = bestFood.first;
                    int foodColumn = bestFood.second;

                    int firstMoveHomeCost = 1 + std::abs(
                        this->terrainMap[nextRow][nextColumn] -
                        this->terrainMap[foodRow][foodColumn]
                    );

                    int energyAfterReachingFood =
                        ant.energy - bestCostToFood;

                    if (energyAfterReachingFood >= firstMoveHomeCost)
                    {
                        ant.move(
                            this->terrainMap,
                            bestFood,
                            this->foodMap
                        );

                        ant.returnHome(
                            this->terrainMap,
                            this->foodMap
                        );

                        continue;
                    }
                }

                // Food is visible but cannot be usefully moved by this ant.
                if (ant.pheromoneDropped == false &&
                    this->ants.size() > 1 &&
                    this->pheromoneMap[ant.position.first]
                                      [ant.position.second] == 0)
                {
                    ant.dropPheromone(this->pheromoneMap);
                }

                // With other ants alive, leave this food for a potential responder.
                // A lone ant can still try exploring rather than waiting forever.
                if (this->ants.size() > 1)
                {
                    continue;
                }
            }
        }

        // No higher-priority food job: check for another ant's pheromone.
        bool assignedPheromone = false;

        for (int ownerIndex = 0;
             ownerIndex < static_cast<int>(this->ants.size());
             ownerIndex++)
        {
            Ant &owner = this->ants[ownerIndex];

            if (owner.pheromoneDropped == false)
            {
                continue;
            }

            int bestResponderIndex = -1;
            int bestPheromoneCost = 0;

            for (int responderIndex = 0;
                 responderIndex < static_cast<int>(this->ants.size());
                 responderIndex++)
            {
                Ant &possibleResponder =
                    this->ants[responderIndex];

                if (responderIndex == ownerIndex ||
                    possibleResponder.carryingFood == true ||
                    possibleResponder.energy <= 0)
                {
                    continue;
                }

                // A nearly exhausted marker owner must not abandon its own marker.
                if (possibleResponder.pheromoneDropped == true &&
                    possibleResponder.energy <= 1)
                {
                    continue;
                }

                // Visible food takes priority over responding to a pheromone.
                if (possibleResponder.foodScan(this->foodMap).empty() == false)
                {
                    continue;
                }

                std::vector<Coord> pathToPheromone =
                    shortestPath(
                        this->terrainMap,
                        possibleResponder.position,
                        owner.pheromonePosition
                    );

                if (pathToPheromone.empty() == true)
                {
                    continue;
                }

                int pheromoneCost =
                    calculatePathCost(
                        this->terrainMap,
                        pathToPheromone
                    );

                if (possibleResponder.energy >= pheromoneCost &&
                    (bestResponderIndex == -1 ||
                     pheromoneCost < bestPheromoneCost))
                {
                    bestResponderIndex = responderIndex;
                    bestPheromoneCost = pheromoneCost;
                }
            }

            if (bestResponderIndex == antIndex)
            {
                assignedPheromone = true;
                Coord pheromoneTarget =
                    owner.pheromonePosition;

                ant.move(
                    this->terrainMap,
                    pheromoneTarget,
                    this->foodMap
                );

                if (ant.position == pheromoneTarget)
                {
                    owner.erasePheromone(this->pheromoneMap);
                }

                break;
            }
        }

        // The responder scans its destination on the next simulation step.
        if (assignedPheromone == true)
        {
            continue;
        }

        if (ant.energy > 0)
        {
            freeAntIndices.push_back(antIndex);
        }
    }

    // Print the low-energy diagnostic only once, not every simulation step.
    if (printedStuckAnt == false &&
        this->ants.size() == 1 &&
        this->ants[0].energy <= 2)
    {
        Ant &ant = this->ants[0];

        std::vector<Coord> nearbyFood =
            ant.foodScan(this->foodMap);

        std::cout << "\nSTUCK ANT DIAGNOSTIC\n";
        std::cout << "Energy: " << ant.energy << '\n';
        std::cout << "Carrying food: "
                  << ant.carryingFood << '\n';
        std::cout << "Owns pheromone: "
                  << ant.pheromoneDropped << '\n';
        std::cout << "Visible food count: "
                  << nearbyFood.size() << '\n';
        std::cout << "Free ants: "
                  << freeAntIndices.size() << '\n';
        std::cout << "Score so far: "
                  << this->score << '\n';

        printedStuckAnt = true;
    }

    // Lower-energy free ants choose cheap exploration targets first.
    std::sort(
        freeAntIndices.begin(),
        freeAntIndices.end(),
        [this](int firstIndex, int secondIndex)
        {
            return this->ants[firstIndex].energy <
                   this->ants[secondIndex].energy;
        }
    );

    std::vector<Coord> claimedExplorationTargets;

    for (int freeIndex : freeAntIndices)
    {
        Ant &exploringAnt = this->ants[freeIndex];

        bool targetFound = false;
        Coord bestTarget;
        int bestTargetCost = 0;

        // First try spaced-out targets. Relax spacing if all are reserved.
        for (int spacingPass = 0;
             spacingPass < 2 && targetFound == false;
             spacingPass++)
        {
            for (int row = 0;
                 row < static_cast<int>(scannedMap.size());
                 row++)
            {
                for (int column = 0;
                     column < static_cast<int>(scannedMap[0].size());
                     column++)
                {
                    if (scannedMap[row][column] != 0)
                    {
                        continue;
                    }

                    if (spacingPass == 0)
                    {
                        bool tooCloseToClaimedTarget = false;

                        for (Coord claimedTarget :
                             claimedExplorationTargets)
                        {
                            int targetSpacing =
                                std::abs(
                                    row - claimedTarget.first
                                ) +
                                std::abs(
                                    column - claimedTarget.second
                                );

                            if (targetSpacing < 5)
                            {
                                tooCloseToClaimedTarget = true;
                                break;
                            }
                        }

                        if (tooCloseToClaimedTarget == true)
                        {
                            continue;
                        }
                    }

                    Coord candidateTarget =
                        Coord(row, column);

                    std::vector<Coord> pathToTarget =
                        shortestPath(
                            this->terrainMap,
                            exploringAnt.position,
                            candidateTarget
                        );

                    // The first step must be affordable even for a partial trip.
                    if (pathToTarget.size() < 2)
                    {
                        continue;
                    }

                    Coord firstStep = pathToTarget[1];

                    int firstStepCost = 1 + std::abs(
                        this->terrainMap
                            [firstStep.first]
                            [firstStep.second] -
                        this->terrainMap
                            [exploringAnt.position.first]
                            [exploringAnt.position.second]
                    );

                    if (exploringAnt.energy < firstStepCost)
                    {
                        continue;
                    }

                    int targetCost =
                        calculatePathCost(
                            this->terrainMap,
                            pathToTarget
                        );

                    if (targetFound == false ||
                        targetCost < bestTargetCost)
                    {
                        bestTarget = candidateTarget;
                        bestTargetCost = targetCost;
                        targetFound = true;
                    }
                }
            }
        }

        if (targetFound == true)
        {
            claimedExplorationTargets.push_back(
                bestTarget
            );

            exploringAnt.move(
                this->terrainMap,
                bestTarget,
                this->foodMap
            );
        }
    }
}

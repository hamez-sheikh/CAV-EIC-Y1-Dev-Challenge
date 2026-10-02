
//
// Created by dusan on 9/15/26.
//

#include "../include/antworld.h"
#include <algorithm>
#include <cmath>
#include <vector>

void AntWorld::forage()
{
    static AntWorld *lastWorld = nullptr;
    static MapTemplate scannedMap;

    // Ants with no affordable delivery this step. They will explore instead
    std::vector<int> freeAntIndices;

    if (lastWorld != this ||
        scannedMap.size() != this->foodMap.size() ||
        // (scannedMap.empty() == false &&
         scannedMap[0].size() != this->foodMap[0].size()))
    {
        scannedMap = MapTemplate(
            this->foodMap.size(),
            std::vector<int>(this->foodMap[0].size(), 0)
        );

        lastWorld = this;
    }

    // First pass: carry food home, otherwise deliver the cheapest remembered food.
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

        // Scan, then update the memory map.
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

        // Re-mark every cell where this ant's sensor actually saw food.
        for (Coord food : visibleFood)
        {
            scannedMap[food.first][food.second] = 2;
        }
        // Consider every cell remembered as holding food, not only what this ant sees.
        bool deliveryFound = false;
        Coord bestDeliveryFood;
        int bestDeliveryCost = 0;

        for (int row = 0;
             row < static_cast<int>(scannedMap.size());
             row++) {
            for (int column = 0;
                 column < static_cast<int>(scannedMap[0].size());
                 column++) {
                if (scannedMap[row][column] != 2)
                {
                    continue;
                }

                // Every edge costs at least 1, so path cost >= Manhattan distance.
                int distanceToFood =
                    std::abs(row - ant.position.first) +
                    std::abs(column - ant.position.second);

                if (distanceToFood > ant.energy)
                {
                    continue;
                }

                Coord food = Coord(row, column);

                std::vector<Coord> pathToFood =
                    shortestPath(this->terrainMap, ant.position, food);

                std::vector<Coord> pathFoodToHome =
                    shortestPath(this->terrainMap, food, ant.homeCoord);

                if (pathToFood.empty() == true ||
                    pathFoodToHome.empty() == true)
                {
                    continue;
                }

                int costToFood =
                    calculatePathCost(this->terrainMap, pathToFood);

                int costFoodToHome =
                    calculatePathCost(this->terrainMap, pathFoodToHome);

                if (ant.energy >= costToFood + costFoodToHome)
                {
                    int totalCost = costToFood + costFoodToHome;

                    if (deliveryFound == false ||
                        totalCost < bestDeliveryCost)
                    {
                        bestDeliveryFood = food;
                        bestDeliveryCost = totalCost;
                        deliveryFound = true;
                    }
                }
            }
        }

        if (deliveryFound == true)
        {
            scannedMap[bestDeliveryFood.first][bestDeliveryFood.second] = 1;

            ant.move(this->terrainMap, bestDeliveryFood, this->foodMap);
            ant.returnHome(this->terrainMap, this->foodMap);
            continue;
        }

        if (ant.energy > 0)
        {
            freeAntIndices.push_back(antIndex);
        }
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

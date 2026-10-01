//
// Created by dusan on 9/15/26.
//

#include "../include/antworld.h"


/** @brief this is where you as the applicant will make use of the above functions to develop your solution.
 * here are some existing examples of how calling these functions works to help get you started!
 */
void AntWorld::forage()
{
    static bool initialized = false;
    static MapTemplate scannedMap;

    // one time setup
    if (!initialized)
    {
        scannedMap = MapTemplate(
            this->foodMap.size(),
            std::vector<int>(this->foodMap[0].size(),0)
            );
        initialized = true;
    }

// proccesses each ant
    for (int antIndex = 0; antIndex < static_cast<int>(this->ants.size());
    antIndex++)
    { Ant &ant = this->ants[antIndex];
        // protects ants that have an active pheromone and no where to go
        if (ant.pheromoneDropped == true && ant.energy <= 1)
        {
            continue;
        }
        // get food home ASAP before doing anything else
        if (ant.carryingFood == true)
        {
            ant.returnHome(this->terrainMap, this->foodMap);
            continue;
        }

        bool assignedPheromone = false;
        bool reachedPheromone = false;
        // look for all pheromones
    for (int ownerIndex = 0; ownerIndex < static_cast<int>(this->ants.size());
        ownerIndex++)
    {
        Ant &owner = this->ants[ownerIndex];

        if (owner.pheromoneDropped == true)
        {// start with no responder
            int bestResponderIndex = -1;
            int bestPheromoneCost = 0;
            // check every possible responder
            for (int responderIndex = 0; responderIndex < static_cast<int>(this->ants.size());
                responderIndex++)
            {
                Ant &possibleResponder = this->ants[responderIndex];
                // ensures the owner of the pheromone doesnt respond to himself
                if (responderIndex == ownerIndex)
                {
                    continue;
                }
                // if an ant is carrying food, skip it
                if (possibleResponder.carryingFood == true)
                {
                    continue;
                }

                std::vector<Coord> pathToPheromone = shortestPath(this->terrainMap, possibleResponder.position, owner.pheromonePosition);

                int pheromoneCost = calculatePathCost(this->terrainMap, pathToPheromone);
                // make sure it can actually reach the pheromone
                if (possibleResponder.energy >= pheromoneCost)
                {
                    if (bestResponderIndex == -1 || pheromoneCost < bestPheromoneCost)
                    {
                        bestResponderIndex = responderIndex;
                        bestPheromoneCost = pheromoneCost;
                    }
                }
            }
            if (bestResponderIndex == antIndex)
            {
                assignedPheromone = true;
                Coord pheromoneTarget = owner.pheromonePosition;

                ant.move(this->terrainMap, pheromoneTarget,this->foodMap);
                if (ant.position == pheromoneTarget)
                {
                    owner.erasePheromone(this->pheromoneMap);
                    reachedPheromone = true;
                }
                break;
            }
            if (assignedPheromone == true && reachedPheromone == false)
            {
                continue;
            }
        }
    }



        //scan for nearby food
        std::vector<Coord> visibleFood = ant.foodScan(this->foodMap);
        // goes through every row the ant can see
        for (int row = ant.position.first - ant.foodRadius;
            row <= ant.position.first + ant.foodRadius;
            row++)
        { // goes through every column the ant can see
            for (int column = ant.position.second - ant.foodRadius;
                column <= ant.position.second + ant.foodRadius;
                column++)
            {// makes sure the cell being scanned is ON the map and if it is, it is marked as scanned
                if (row >= 0 && row < static_cast<int>(scannedMap.size())
                    && column >= 0 && column < static_cast<int>(scannedMap.size()))
                {
                    scannedMap[row][column] = 1;
                }
            }
        }
        if (visibleFood.empty() == false)
        {
            bool foodChosen = false;
            Coord bestFood;
            int bestTotalCost = 0;
            int bestCostToFood = 0;
            int bestCostFoodtoHome = 0;
            std::vector<Coord> bestPathFoodToHome;

            for (Coord food : visibleFood) {
                std::vector<Coord> pathToFood = shortestPath(this->terrainMap, ant.position, food);

                int costToFood = calculatePathCost(this->terrainMap, pathToFood);

                std::vector<Coord> pathFoodToHome = shortestPath(this->terrainMap, food, ant.homeCoord);

                int costFoodToHome = calculatePathCost(this->terrainMap, pathFoodToHome);

                int totalDeliveryCost = costToFood + costFoodToHome;

                if (foodChosen == false || totalDeliveryCost < bestTotalCost)
                {
                    bestFood = food;
                    bestTotalCost = totalDeliveryCost;
                    bestCostToFood = costToFood;
                    bestCostFoodtoHome = costFoodToHome;
                    bestPathFoodToHome = pathFoodToHome;
                    foodChosen = true;
                }
            }
            if (foodChosen == true)
            {// if the ant has enough energy to deliver the food home, let it cook
                if (ant.energy >= bestTotalCost)
                {
                    ant.move(this->terrainMap, bestFood, this-> foodMap);
                    ant.returnHome(this->terrainMap, this->foodMap);
                    continue;
                }// check if the ant has enough energy to reach the food and move one step closer to home
                else if ( ant.energy >= bestCostToFood && bestPathFoodToHome.size() >= 2) {
                    int nextRow = bestPathFoodToHome[1].first;
                    int nextColumn = bestPathFoodToHome[1].second;

                    int foodRow = bestFood.first;
                    int foodColumn = bestFood.second;

                    int firstMoveHomeCost =
                        1 + std::abs (
                        this->terrainMap[nextRow][nextColumn] - this->terrainMap[foodRow][foodColumn]);
                    int energyAfterReachingFood = ant.energy - bestCostToFood;
                    // if the ant can actually take a meaningful step towards home, let it
                    if (energyAfterReachingFood >= firstMoveHomeCost)
                    {
                        ant.move(this->terrainMap, bestFood, this-> foodMap);
                        ant.returnHome(this->terrainMap, this->foodMap);
                        continue;
                    }
                }
                if (ant.pheromoneDropped == false)
                {
                    ant.dropPheromone(this->pheromoneMap);
                }
                continue;
            }
        }
        //find nearby unscanned territory
    }
}

    // std::vector<Coord> visibleFood = this->ants[0].foodScan(this->foodMap);
    //
    // Coord desiredDestination = Coord(5, 5);
    // Coord finalPos = this->ants[0].move(this->terrainMap, desiredDestination, this->foodMap);
    // bool destCheck = (desiredDestination == finalPos);
    //
    // this->ants[0].dropPheromone(this->pheromoneMap);
    //
    // this->ants[0].erasePheromone(this->pheromoneMap);
    //
    // this->ants[0].returnHome(this->terrainMap, this->foodMap);


/** You may insert any custom functions below **/

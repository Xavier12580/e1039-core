//-----------------------------------------------------------------------------
//
//  The PHOOL's Software
//  Copyright (C) PHENIX collaboration, 1999
//
//  Implementation of class PHNodeIterator
//
//  Author: Matthias Messer
//-----------------------------------------------------------------------------
#include "PHNodeIterator.h"
#include "PHPointerListIterator.h"
#include "PHNodeOperation.h"
#include "phooldefs.h"

#include <boost/algorithm/string.hpp>

#include <iostream>
#include <vector>

using namespace std;

PHNodeIterator::PHNodeIterator(PHCompositeNode* node):
  currentNode(node)
{}

PHNodeIterator::PHNodeIterator():
  currentNode(NULL)
{}

PHPointerList<PHNode>&
PHNodeIterator::ls()
{
  PHPointerListIterator<PHNode> iter(currentNode->subNodes);
  subNodeList.clear();
  PHNode *thisNode;
  while ((thisNode = iter()))
    {
      subNodeList.append(thisNode);
    }
  return subNodeList;
}

void
PHNodeIterator::print()
{
  currentNode->print();
}

PHNode*
PHNodeIterator::findFirst(const string& requiredType, const string& requiredName)
{
  PHPointerListIterator<PHNode> iter(currentNode->subNodes);
  PHNode *thisNode;
  while ((thisNode = iter()))
    {
      if (thisNode->getType() == requiredType && thisNode->getName() == requiredName)
        {
          return thisNode;
        }
      else
        {
          if (thisNode->getType() == "PHCompositeNode")
            {
              PHNodeIterator nodeIter(static_cast<PHCompositeNode*>(thisNode));
              PHNode *nodeFoundInSubTree = nodeIter.findFirst(requiredType.c_str(), requiredName.c_str());
              if (nodeFoundInSubTree) return nodeFoundInSubTree;
            }
        }
    }
  return 0;
}

PHNode*
PHNodeIterator::findFirst(const string& requiredName)
{
  PHPointerListIterator<PHNode> iter(currentNode->subNodes);
  PHNode *thisNode;
  while ((thisNode = iter()))
    {
      if (thisNode->getName() == requiredName)
        {
          return thisNode;
        }
      else
        {
          if (thisNode->getType() == "PHCompositeNode")
            {
              PHNodeIterator nodeIter(static_cast<PHCompositeNode*>(thisNode));
              PHNode *nodeFoundInSubTree = nodeIter.findFirst(requiredName.c_str());
              if (nodeFoundInSubTree)
                {
                  return nodeFoundInSubTree;
                }
            }
        }
    }
  return 0;
}

PHBoolean
PHNodeIterator::cd(const string &pathString)
{
  PHBoolean success = True;
  if (pathString.empty())
    {
      while (currentNode->getParent())
        {
          currentNode = static_cast<PHCompositeNode*>(currentNode->getParent());
        }
    }
  else
    {
      vector<string> splitpath;
      boost::split(splitpath, pathString, boost::is_any_of(phooldefs::nodetreepathdelim));
      PHBoolean pathFound;
      PHNode    *subNode;
      int i=0;
      for (vector<string>::const_iterator iter = splitpath.begin(); iter != splitpath.end(); ++iter)
        {
	  i++;
          if (*iter == "..")
            {
              if (currentNode->getParent())
                {
                  currentNode = static_cast<PHCompositeNode*>(currentNode->getParent());
                }
              else
                {
                  success = False;
                }
            }
          else
            {
              PHPointerListIterator<PHNode> subNodeIter(currentNode->subNodes);
              pathFound = False;
              while ((subNode = subNodeIter()))
                {
                  if (subNode->getType() == "PHCompositeNode" && subNode->getName() == *iter)
                    {
                      currentNode = static_cast<PHCompositeNode*>(subNode);
                      pathFound = True;
                    }
                }
              if (!pathFound)
                {
                  success = False;
                }
            }
        }
    }
  return success;
}

PHBoolean
PHNodeIterator::addNode(PHNode* newNode)
{
  return currentNode->addNode(newNode);
}

void
PHNodeIterator::forEach(PHNodeOperation& operation)
{
  operation(currentNode);
  PHPointerListIterator<PHNode> iter(currentNode->subNodes);
  PHNode *thisNode;
  while ((thisNode = iter()))
    {
      if (thisNode->getType() == "PHCompositeNode")
        {
          PHNodeIterator subNodeIter(static_cast<PHCompositeNode*>(thisNode));
          subNodeIter.forEach(operation);
        }
      else
        {
          operation(thisNode);
        }
    }
}

void
PHNodeIterator::for_each(PHNodeOperation& operation)
{
  forEach(operation);
}

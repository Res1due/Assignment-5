/**
 * Andersen.cpp
 * @author kisslune
 */

#include "A5Header.h"

using namespace llvm;
using namespace std;

int main(int argc, char** argv)
{
    auto moduleNameVec =
            OptionBase::parseOptions(argc, argv, "Whole Program Points-to Analysis",
                                     "[options] <input-bitcode...>");

    SVF::LLVMModuleSet::buildSVFModule(moduleNameVec);

    SVF::SVFIRBuilder builder;
    auto pag = builder.build();
    auto consg = new SVF::ConstraintGraph(pag);
    consg->dump();

    Andersen andersen(consg);
    auto cg = pag->getCallGraph();

    andersen.runPointerAnalysis();
    andersen.updateCallGraph(cg);

    cg->dump();
    SVF::LLVMModuleSet::releaseLLVMModuleSet();
	return 0;
}


void Andersen::runPointerAnalysis()
{
    WorkList<unsigned> workList;
    pts.clear();

    // p = &o: o belongs to PTS(p).
    for (const auto *edge : consg->getAddrCGEdges())
        if (pts[edge->getDstID()].insert(edge->getSrcID()).second)
            workList.push(edge->getDstID());

    while (!workList.empty())
    {
        const unsigned id = workList.pop();
        const auto *node = consg->getConstraintNode(id);
        // Propagation may change this set through self edges; process a snapshot.
        const auto pointees = pts[id];

        for (unsigned object : pointees)
        {
            // q = *p: for o in PTS(p), introduce o --Copy--> q.
            for (const auto *edge : node->getLoadOutEdges())
                if (consg->addCopyCGEdge(object, edge->getDstID()))
                    workList.push(object);

            // *p = q: for o in PTS(p), introduce q --Copy--> o.
            for (const auto *edge : node->getStoreInEdges())
                if (consg->addCopyCGEdge(edge->getSrcID(), object))
                    workList.push(edge->getSrcID());
        }

        // q = p: PTS(p) is a subset of PTS(q).
        for (const auto *edge : node->getCopyOutEdges())
        {
            auto &destination = pts[edge->getDstID()];
            const auto oldSize = destination.size();
            destination.insert(pointees.begin(), pointees.end());
            if (destination.size() != oldSize)
                workList.push(edge->getDstID());
        }

        // q = &p->field: preserve distinct constant-offset field objects.
        for (const auto *edge : node->getGepOutEdges())
        {
            const auto *gep = SVF::SVFUtil::cast<SVF::GepCGEdge>(edge);
            for (unsigned object : pointees)
            {
                const unsigned field = consg->getGepObjVar(object, gep);
                if (pts[edge->getDstID()].insert(field).second)
                    workList.push(edge->getDstID());
            }
        }
    }
}


void Andersen::updateCallGraph(SVF::CallGraph* cg)
{
    auto pag = SVF::PAG::getPAG();
    const auto addCopy = [this](unsigned src, unsigned dst) {
        // ConsG can omit isolated actual/formal and return nodes.
        if (!consg->hasConstraintNode(src))
            consg->addConstraintNode(new SVF::ConstraintNode(src), src);
        if (!consg->hasConstraintNode(dst))
            consg->addConstraintNode(new SVF::ConstraintNode(dst), dst);
        consg->addCopyCGEdge(src, dst);
    };
    std::set<std::pair<const SVF::CallICFGNode*, const SVF::FunObjVar*>> resolved;
    bool changed;
    do
    {
        changed = false;
        for (const auto &entry : pag->getIndirectCallsites())
        {
            const auto *call = entry.first;
            const auto targets = pts[entry.second];
            for (unsigned target : targets)
            {
                const auto *function = SVF::SVFUtil::dyn_cast<SVF::FunObjVar>(pag->getGNode(target));
                if (!function || !resolved.insert({call, function}).second)
                    continue;
                cg->addIndirectCallGraphEdge(call, call->getFun(), function);
                changed = true;

                // Feed actual/formal parameter and return constraints back into PTA.
                if (pag->hasCallSiteArgsMap(call) && pag->hasFunArgsList(function))
                {
                    const auto &actuals = pag->getCallSiteArgsList(call);
                    const auto &formals = pag->getFunArgsList(function);
                    for (size_t i = 0; i < actuals.size() && i < formals.size(); ++i)
                        addCopy(actuals[i]->getId(), formals[i]->getId());
                }
                const auto *ret = call->getRetICFGNode();
                if (pag->callsiteHasRet(ret) && pag->funHasRet(function))
                    addCopy(pag->getFunRet(function)->getId(),
                                        pag->getCallSiteRet(ret)->getId());
            }
        }
        if (changed)
            runPointerAnalysis();
    } while (changed);
}

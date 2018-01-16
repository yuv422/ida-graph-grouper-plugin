/*
 * Automatically group a set of nodes starting at a given address.
 * We group all successor nodes that are dominated by the start node
 * until we hit an end node. 
 *
 * End nodes are marked with the comment "GG:stop" in the first address of the node.
 */

#include <vector>

#include <pro.h>
#include <idp.hpp>
#include <graph.hpp>
#include <loader.hpp>

class bitsett : public std::vector<bool>
{
public:

    bitsett &operator|=(const bitsett &rhs)
    {
        for (int i = 0; i < size(); i++)
        {
            if (at(i) == false && rhs.at(i) == true)
                at(i) = true;
        }

        return *this;
    }

    bitsett &operator&=(const bitsett &rhs)
    {
        for (int i = 0; i < size(); i++)
        {
            if (at(i) == true && rhs.at(i) == false)
                at(i) = false;
        }

        return *this;
    }

    bitsett &set(void)
    {
        int s = size();
        clear();
        assign(s, true);

        return *this;
    }

    bitsett &set(int n)
    {
        at(n) = true;
        return *this;
    }

    bitsett &reset(void)
    {
        int s = size();
        clear();
        assign(s, false);

        return *this;
    }

    bool test(int n)
    {
        return at(n);
    }
};

class DominatorInfo
{
private:
    std::vector<bitsett> doms;
public:
    DominatorInfo(mutable_graph_t *graph)
    {
        int numNodes = graph->size();

        doms.resize(numNodes);

        for (node_iterator iterator = graph->begin(); iterator != graph->end(); ++iterator)
        {
            int node = *iterator;
            doms[node].resize(numNodes);
            doms[node].set();
        }

        doms[graph->entry()].reset();
        doms[graph->entry()].set(graph->entry());

        bitsett t;
        t.resize(numNodes);
        t.reset();

        bool changed;
        do
        {
            changed = false;
            for (node_iterator iterator = graph->begin(); iterator != graph->end(); ++iterator)
            {
                int node = *iterator;
                if (node == graph->entry()) //skip entry node
                    continue;

                for (int p = 0; p < graph->npred(node); p++)
                {
                    int pred = graph->pred(node, p);
                    t.reset();
                    t |= doms[node];
                    doms[node] &= doms[pred];
                    doms[node].set(node);

                    if (doms[node] != t)
                        changed = true;
                }
            }
        } while (changed);

    }

//Does node dominate node2?
    bool dominates_node(int node, int node2)
    {
        return doms[node2].test(node);
    }

};

//--------------------------------------------------------------------------
int init(void)
{
    return is_idaq() ? PLUGIN_OK : PLUGIN_SKIP;
}

//--------------------------------------------------------------------------
void term(void)
{
}


bool search_comment(ea_t ea, const char *searchString)
{
    qstring cmt;

    if (get_cmt(&cmt, ea, false) == -1)
        return false;

    if (strstr(cmt.c_str(), searchString) != NULL)
        return true;

    return false;
}

bool is_end_node(mutable_graph_t *graph, int node)
{
    intvec_t nodes;
    nodes.push_back(node);

    ea_t groupEa = graph->calc_group_ea(nodes);

    return search_comment(groupEa, "GG:stop");
}

void add_nodes(mutable_graph_t *graph, DominatorInfo *d, intvec_t &nodes, int startNode, int node)
{
    if (is_end_node(graph, node))
        return;

    nodes.push_back(node);
    for (int s = 0; s < graph->nsucc(node); s++)
    {
        int succ = graph->succ(node, s);

        //Add succ node if it hasn't been added already and if it is dominated by the startNode.
        if (!nodes.has(succ) && d->dominates_node(startNode, succ))
        {
            add_nodes(graph, d, nodes, startNode, succ);
        }
    }

}

qstring getGroupText(mutable_graph_t *g, int selectedNode)
{
    qstring textBuffer;
    qstring nodeComment;

    intvec_t node_set;
    node_set.push_back(selectedNode);

    ea_t nodeAddr = g->calc_group_ea(node_set);

    if (get_cmt(&nodeComment, nodeAddr, false) == -1
        && get_cmt(&nodeComment, nodeAddr, true) == -1)
    {
        if (!ask_text(&textBuffer, 2048, "group text", "Please enter group text"))
            return qstring("");
    }
    else
    {
        if (!ask_text(&textBuffer, 2048, nodeComment.c_str(), "Please enter group text"))
            return qstring("");
    }

    return textBuffer;
}

//--------------------------------------------------------------------------
bool run(size_t /*arg*/)
{
    graph_viewer_t *graphViewer = get_current_viewer(); //get_graph_viewer(form);
    if (graphViewer == NULL)
    {
        msg("graphViewer is null");
        return false;
    }

    mutable_graph_t *graph = get_viewer_graph(graphViewer);
    if (graph == NULL)
    {
        msg("graph is null");
        return false;
    }

    DominatorInfo dominatorInfo(graph);

    msg("graph size = %d cur_node =%d\n", graph->size(), viewer_get_curnode(graphViewer));

    int startNode = viewer_get_curnode(graphViewer);

    if (startNode == -1)
    {
        warning("Please select a node to start grouping from.");
        return false;
    }

    qstring groupText = getGroupText(graph, startNode);

    if (groupText.length() == 0)
    {
        msg("Cancelling as no group text was entered.\n");
        return false;
    }
    intvec_t nodeSet;

    if (startNode >= 0)
    {
        add_nodes(graph, &dominatorInfo, nodeSet, startNode, startNode);

        intvec_t out_groups;
        groups_crinfos_t groups;
        group_crinfo_t group;
        group.nodes = nodeSet;
        group.text = groupText;

        groups.push_back(group);
        viewer_create_groups(graphViewer, &out_groups, groups);
    }

    return true;
}

//--------------------------------------------------------------------------
char comment[] = "Graph Grouper";

char help[] =
        "Group all the dominated nodes from a given node in the graph viewer.\n";


//--------------------------------------------------------------------------
// This is the preferred name of the plugin module in the menu system
// The preferred name may be overriden in plugins.cfg file

char wanted_name[] = "graph grouper";


// This is the preferred hotkey for the plugin module
// The preferred hotkey may be overriden in plugins.cfg file
// Note: IDA won't tell you if the hotkey is not correct
//       It will just disable the hotkey.

char wanted_hotkey[] = "Ctrl-5";


//--------------------------------------------------------------------------
//
//      PLUGIN DESCRIPTION BLOCK
//
//--------------------------------------------------------------------------
plugin_t PLUGIN =
        {
                IDP_INTERFACE_VERSION,
                0,                    // plugin flags
                init,                 // initialize

                term,                 // terminate. this pointer may be NULL.

                run,                  // invoke plugin

                comment,              // long comment about the plugin
                // it could appear in the status line
                // or as a hint

                help,                 // multiline help about the plugin

                wanted_name,          // the preferred short name of the plugin
                wanted_hotkey         // the preferred hotkey to run the plugin
        };

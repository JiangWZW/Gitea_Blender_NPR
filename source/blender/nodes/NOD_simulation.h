#ifndef __NOD_SIMULATION_H__
#define __NOD_SIMULATION_H__

#ifdef __cplusplus
extern "C" {
#endif

extern struct bNodeTreeType *ntreeType_Simulation;

void register_node_tree_type_sim(void);

void register_node_type_sim_group(void);

#ifdef __cplusplus
}
#endif

#endif /* __NOD_SIMULATION_H__ */

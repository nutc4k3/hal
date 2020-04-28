//  MIT License
//
//  Copyright (c) 2019 Ruhr-University Bochum, Germany, Chair for Embedded Security. All Rights reserved.
//  Copyright (c) 2019 Marc Fyrbiak, Sebastian Wallat, Max Hoffmann ("ORIGINAL AUTHORS"). All rights reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.

#pragma once

#include "core/log.h"
#include "def.h"
#include "netlist/gate_library/gate_type/gate_type.h"
#include "netlist/module.h"
#include "netlist/net.h"
#include "netlist/netlist.h"
#include "netlist/netlist_factory.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

/* forward declaration*/
class netlist;

/**
 * @ingroup hdl_parsers
 */
template<typename T>
class HDL_PARSER_API hdl_parser
{
public:
    /**
     * @param[in] stream - The string stream filled with the hdl code.
     */
    explicit hdl_parser(std::stringstream& stream) : m_fs(stream)
    {
        m_netlist = nullptr;
    }

    virtual ~hdl_parser() = default;

    /**
     * TODO 
     * Parses hdl code for a specific netlist library.
     *
     * @param[in] gate_library - The gate library name.
     * @returns The netlist representation of the hdl code or a nullptr on error.
     */
    virtual bool parse() = 0;

    // TODO
    std::shared_ptr<netlist> instantiate(const std::string& gate_library)
    {
        // create empty netlist
        m_netlist = netlist_factory::create_netlist(gate_library);
        auto& tmp = m_netlist->get_gate_library()->get_gate_types();
        std::map<T, std::shared_ptr<const gate_type>> gate_types;
        for (const auto& test : tmp)
        {
            gate_types.emplace(T(test.first.data()), test.second);
        }

        if (m_netlist == nullptr)
        {
            // error printed in subfunction
            return nullptr;
        }

        for (auto& e : m_entities)
        {
            for (auto& inst : e.second._instances)
            {
                std::map<T, std::pair<signal, std::vector<signal>>>& port_assignments = inst.second._port_assignments;

                // instance of entity
                if (auto entity_it = m_entities.find(inst.second._type); entity_it != m_entities.end())
                {
                    // fill in port width
                    auto& entity_ports = entity_it->second._ports;

                    for (auto& [port_name, assignment] : port_assignments)
                    {
                        if (!assignment.first._is_ranges_known)
                        {
                            if (auto port_it = entity_ports.find(port_name); port_it != entity_ports.end())
                            {
                                assignment.first.set_ranges(port_it->second.second._ranges);

                                i32 left_size  = assignment.first.size();
                                i32 right_size = 0;
                                for (const auto& s : assignment.second)
                                {
                                    right_size += s.size();
                                }

                                if (left_size != right_size)
                                {
                                    log_error("hdl_parser",
                                              "port assignment width mismatch: left side has size {} and right side has size {} in line {}.",
                                              left_size,
                                              right_size,
                                              assignment.first._line_number);
                                    return nullptr;
                                }
                            }
                            else
                            {
                                log_error(
                                    "hdl_parser", "port '{}' is no valid port for instance '{}' of entity '{}' in line {}.", port_name, inst.first, entity_it->first, assignment.first._line_number);
                                return nullptr;
                            }
                        }
                    }
                }
                // instance of gate type
                else if (auto gate_it = gate_types.find(inst.second._type); gate_it != gate_types.end())
                {
                    std::map<T, std::vector<u32>> pin_groups;
                    for (const auto& pin_group : gate_it->second->get_input_pin_groups())
                    {
                        pin_groups.emplace(T(pin_group.first.data()), pin_group.second);
                    }
                    for (const auto& pin_group : gate_it->second->get_output_pin_groups())
                    {
                        pin_groups.emplace(T(pin_group.first.data()), pin_group.second);
                    }

                    for (auto& [port_name, assignment] : port_assignments)
                    {
                        if (auto pin_it = pin_groups.find(port_name); pin_it != pin_groups.end())
                        {
                            if (!pin_it->second.empty())
                            {
                                assignment.first.set_ranges({pin_it->second});
                            }
                            else
                            {
                                assignment.first._is_ranges_known = true;
                            }
                        }
                        else
                        {
                            log_error("hdl_parser", "pin '{}' is no valid pin for gate '{}' of type '{}' in line {}.", port_name, inst.first, gate_it->first, assignment.first._line_number);
                            return nullptr;
                        }

                        i32 left_size  = assignment.first.size();
                        i32 right_size = 0;
                        for (const auto& s : assignment.second)
                        {
                            right_size += s.size();
                        }

                        if (left_size != right_size)
                        {
                            log_error(
                                "hdl_parser", "port assignment width mismatch: port has width {} and assigned signal has width {} in line {}.", left_size, right_size, assignment.first._line_number);
                            return nullptr;
                        }
                    }
                }
                else
                {
                    log_error("hdl_parser", "type '{}' of instance '{}' is neither an entity, nor a gate type in line {}.", inst.second._type, inst.first, inst.second._line_number);
                    return nullptr;
                }
            }
        }

        return nullptr;
    }

protected:
    class signal
    {
    public:
        u32 _line_number;

        // name (may either be the identifier of the signal or a binary string in case of direct assignments)
        T _name;

        // ranges
        std::vector<std::vector<u32>> _ranges;

        // is binary string?
        bool _is_binary = false;

        // are bounds already known? (should only be unknown for left side of port assignments)
        bool _is_ranges_known = true;

        signal(u32 line_number, T name, std::vector<std::vector<u32>> ranges, bool is_binary = false, bool is_ranges_known = true)
            : _line_number(line_number), _name(name), _ranges(ranges), _is_binary(is_binary), _is_ranges_known(is_ranges_known)
        {
        }

        i32 size() const
        {
            if (_is_ranges_known)
            {
                if (_is_binary)
                {
                    return _name.size();
                }
                else if (_ranges.empty())
                {
                    return 1;
                }
                else
                {
                    return _ranges.size();
                }
            }

            return -1;
        }

        void set_ranges(std::vector<std::vector<u32>> ranges)
        {
            _ranges          = ranges;
            _is_ranges_known = true;
        }

        bool operator<(const signal& other) const
        {
            // there may be two assignments to the same signal using different bounds
            // without checking bounds, two such signals would be considered equal
            return (_name < other._name) && (_ranges < other._ranges);
        }
    };

    class instance
    {
    public:
        u32 _line_number;

        // name
        T _name;

        // type
        T _type;

        // port assignments: port_name -> (port_signal, assignment_signals)
        std::map<T, std::pair<signal, std::vector<signal>>> _port_assignments;

        // generic assignments: generic_name -> (data_type, data_value)
        std::map<T, std::pair<T, T>> _generic_assignments;

        bool operator<(const instance& other) const
        {
            return _name < other._name;
        }
    };

    class entity
    {
    public:
        u32 _line_number;

        // name
        T _name;

        // ports: port_name -> (direction, signal)
        std::map<T, std::pair<T, signal>> _ports;

        // expanded ports: port_name -> expanded_ports
        std::map<std::string, std::vector<std::string>> _ports_expanded;

        // attributes: attribute_target -> set(attribute_name, attribute_type, attribute_value)
        std::map<T, std::set<std::tuple<T, T, T>>> _entity_attributes;
        std::map<T, std::set<std::tuple<T, T, T>>> _instance_attributes;
        std::map<T, std::set<std::tuple<T, T, T>>> _signal_attributes;

        // signals: signal_name -> signal
        std::map<T, signal> _signals;

        // expanded signals: signal_name -> expanded_signals
        std::map<T, std::vector<T>> _signals_expanded;

        // assignments: set(lhs, rhs)
        std::set<std::pair<std::vector<signal>, std::vector<signal>>> _assignments;

        // instances: instance_name -> instance
        std::map<T, instance> _instances;

        bool operator<(const entity& other) const
        {
            return _name < other._name;
        }
    };

    // stores the input stream to the file
    std::stringstream& m_fs;

    std::map<T, entity> m_entities;
    T m_last_entity;

private:
    // stores the netlist
    std::shared_ptr<netlist> m_netlist;

    // unique alias generation
    std::map<T, u32> m_signal_name_occurrences;
    std::map<T, u32> m_instance_name_occurrences;
    std::map<T, u32> m_current_signal_index;
    std::map<T, u32> m_current_instance_index;

    // net generation
    std::map<T, std::shared_ptr<net>> m_net_by_name;
    std::map<T, std::vector<T>> m_nets_to_merge;
    std::map<T, std::function<bool(net&)>> mark_global_net_function_map = {{"in", &net::mark_global_input_net}, {"out", &net::mark_global_output_net}};

    // bool build_netlist(const T& top_module)
    // {
    //     m_netlist->set_design_name(top_module);
    //     auto& top_entity = m_entities[top_module];

    //     std::map<std::string, u32> instantiation_count;

    //     // preparations for alias: count the occurences of all names
    //     std::queue<entity*> q;
    //     q.push(&top_entity);

    //     // instance will be named after entity, so take into account for aliases
    //     m_instance_name_occurrences[top_entity._name]++;

    //     // signals will be named after ports, so take into account for aliases
    //     for (const auto& p : top_entity._ports)
    //     {
    //         m_signal_name_occurrences[p.first]++;
    //     }

    //     while (!q.empty())
    //     {
    //         auto e = q.front();
    //         q.pop();

    //         instantiation_count[e->_name]++;

    //         for (const auto& s : e->_signals)
    //         {
    //             m_signal_name_occurrences[s.first]++;
    //         }

    //         for (const auto& inst : e->_instances)
    //         {
    //             m_instance_name_occurrences[inst.first]++;

    //             if (auto it = m_entities.find(inst.second._type); it != m_entities.end())
    //             {
    //                 q.push(&(it->second));
    //             }
    //         }
    //     }

    //     // any unused entities?
    //     for (auto& e : m_entities)
    //     {
    //         if (instantiation_count[e.first] == 0)
    //         {
    //             log_warning("hdl_parser", "entity '{}' is defined but not used", e.first);
    //         }
    //     }

    //     if (instantiate_top_module(top_entity) == nullptr)
    //     {
    //         log_error("hdl_parser", "could not instantiate top module '{}'.", top_entity._name);
    //         return false;
    //     }

    //     // netlist is created.
    //     // now merge nets
    //     while (!m_nets_to_merge.empty())
    //     {
    //         // master = net that other nets are merged into
    //         // slave = net to merge into master and then delete

    //         bool progress_made = false;

    //         for (const auto& [master, merge_set] : m_nets_to_merge)
    //         {
    //             // check if none of the slaves is itself a master
    //             bool okay = true;
    //             for (const auto& slave : merge_set)
    //             {
    //                 if (m_nets_to_merge.find(slave) != m_nets_to_merge.end())
    //                 {
    //                     okay = false;
    //                     break;
    //                 }
    //             }
    //             if (!okay)
    //                 continue;

    //             auto master_net = m_net_by_name.at(master);
    //             for (const auto& slave : merge_set)
    //             {
    //                 auto slave_net = m_net_by_name.at(slave);

    //                 // merge sources
    //                 if (slave_net->is_global_input_net())
    //                 {
    //                     master_net->mark_global_input_net();
    //                 }

    //                 for (const auto& src : slave_net->get_sources())
    //                 {
    //                     slave_net->remove_source(src);

    //                     if (!master_net->is_a_source(src))
    //                     {
    //                         master_net->add_source(src);
    //                     }
    //                 }

    //                 // merge destinations
    //                 if (slave_net->is_global_output_net())
    //                 {
    //                     master_net->mark_global_output_net();
    //                 }

    //                 for (const auto& dst : slave_net->get_destinations())
    //                 {
    //                     slave_net->remove_destination(dst);

    //                     if (!master_net->is_a_destination(dst))
    //                     {
    //                         master_net->add_destination(dst);
    //                     }
    //                 }

    //                 // merge attributes etc.
    //                 for (const auto& it : slave_net->get_data())
    //                 {
    //                     if (!master_net->set_data(std::get<0>(it.first), std::get<1>(it.first), std::get<0>(it.second), std::get<1>(it.second)))
    //                     {
    //                         log_error("hdl_parser", "couldn't set data");
    //                     }
    //                 }

    //                 m_netlist->delete_net(slave_net);
    //                 m_net_by_name.erase(slave);
    //             }

    //             m_nets_to_merge.erase(master);
    //             progress_made = true;
    //             break;
    //         }

    //         if (!progress_made)
    //         {
    //             log_error("hdl_parser", "cyclic dependency between signals found, cannot parse netlist");
    //             return false;
    //         }
    //     }

    //     return true;
    // }

    // std::shared_ptr<module> instantiate_top_module(entity& e)
    // {
    //     std::map<std::string, std::string> top_assignments;

    //     for (const auto& [name, p] : e._ports)
    //     {
    //         std::vector<std::string> expanded_port;
    //         expand_signal_recursively(expanded_port, name, p.second._bounds, 0);
    //         e._ports_expanded.emplace(name, expanded_port);

    //         std::set<std::tuple<std::string, std::string, std::string>> attributes;
    //         if (auto attribute_it = e._signal_attributes.find(name); attribute_it != e._signal_attributes.end())
    //         {
    //             attributes = attribute_it->second;
    //         }

    //         for (const auto& ep : expanded_port)
    //         {
    //             auto net_name = ep;
    //             auto new_net  = m_netlist->create_net(net_name);
    //             if (new_net == nullptr)
    //             {
    //                 log_error("hdl_parser", "could not create new net with name '{}'.", ep);
    //                 return nullptr;
    //             }

    //             m_net_by_name[net_name] = new_net;

    //             // for instances, point the ports to the newly generated signals
    //             top_assignments[net_name] = net_name;

    //             if (!mark_global_net_function_map.at(p.first)(*new_net))
    //             {
    //                 if (p.first == "in")
    //                 {
    //                     log_error("hdl_parser", "could not mark net '{}' with id {} as global input.", net_name, new_net->get_id());
    //                     return nullptr;
    //                 }
    //                 else if (p.first == "out")
    //                 {
    //                     log_error("hdl_parser", "could not mark net '{}' with id {} as global output.", net_name, new_net->get_id());
    //                     return nullptr;
    //                 }
    //             }

    //             // assign signal-level attributes to ports
    //             for (const auto& attr : attributes)
    //             {
    //                 if (!new_net->set_data("attribute", std::get<0>(attr), std::get<1>(attr), std::get<2>(attr)))
    //                 {
    //                     log_error("hdl_parser", "unable to set signal attribute data.");
    //                 }
    //             }
    //         }
    //     }

    //     // create module for top entity
    //     std::shared_ptr<module> m = m_netlist->get_top_module();
    //     if (m == nullptr)
    //     {
    //         log_error("hdl_parser", "could not instantiate top module '{}'.", e._name);
    //         return nullptr;
    //     }

    //     m->set_name(e._name);

    //     // assign entity-level attributes
    //     if (auto attribute_it = e._entity_attributes.find(e._name); attribute_it != e._entity_attributes.end())
    //     {
    //         for (const auto& attr : attribute_it->second)
    //         {
    //             if (!m->set_data("attribute", std::get<0>(attr), std::get<1>(attr), std::get<2>(attr)))
    //             {
    //                 log_error("hdl_parser", "unable to set entity attribute data.");
    //             }
    //         }
    //     }

    //     std::map<std::string, std::string> signal_suffixes;
    //     std::map<std::string, std::string> instance_suffixes;

    //     // create all internal signals
    //     for (const auto& [name, s] : e._signals)
    //     {
    //         // create new net for the signal
    //         signal_suffixes[name] = get_unique_signal_suffix(name);
    //         std::vector<std::string> expanded_signal;
    //         expand_signal_recursively(expanded_signal, name + signal_suffixes[name], s._bounds, 0);
    //         e._signals_expanded.emplace(name, expanded_signal);

    //         for (const auto& es : expanded_signal)
    //         {
    //             auto net_name = es;
    //             auto new_net  = m_netlist->create_net(net_name);
    //             if (new_net == nullptr)
    //             {
    //                 log_error("hdl_parser", "could not create new net with name '{}'.", es);
    //                 return nullptr;
    //             }
    //             m_net_by_name[net_name] = new_net;

    //             // assign signal attributes
    //             if (auto attribute_it = e._signal_attributes.find(name); attribute_it != e._signal_attributes.end())
    //             {
    //                 for (const auto& attr : attribute_it->second)
    //                 {
    //                     if (!new_net->set_data("attribute", std::get<0>(attr), std::get<1>(attr), std::get<2>(attr)))
    //                     {
    //                         log_error("hdl_parser", "unable to set signal attribute data.");
    //                     }
    //                 }
    //             }
    //         }
    //     }

    //     // handle direct assignments
    //     for (const auto& [signals, assignments] : e._assignments)
    //     {
    //         std::list<std::string> lhs;
    //         std::list<std::string> rhs;

    //         for (const auto& s : signals)
    //         {
    //             std::vector<std::string> expanded_signal;
    //             expand_signal_recursively(expanded_signal, s._name + signal_suffixes.at(s._name), s._bounds, 0);
    //             lhs.insert(lhs.end(), expanded_signal.begin(), expanded_signal.end());
    //         }

    //         for (const auto& s : assignments)
    //         {
    //             if (s._is_binary)
    //             {
    //                 std::transform(s._name.begin(), s._name.end(), std::back_inserter(rhs), [](const char c) { return std::to_string(c); });
    //             }
    //             else
    //             {
    //                 std::vector<std::string> expanded_signal;
    //                 expand_signal_recursively(expanded_signal, s._name + signal_suffixes.at(s._name), s._bounds, 0);
    //                 rhs.insert(lhs.end(), expanded_signal.begin(), expanded_signal.end());
    //             }
    //         }

    //         for (auto lhs_it = lhs.begin(), rhs_it = rhs.begin(); lhs_it != lhs.end() && rhs_it != rhs.end(); lhs_it++, rhs_it++)
    //         {
    //             if (top_assignments.find(*lhs_it) != top_assignments.end())
    //             {
    //                 *lhs_it = top_assignments.at(*lhs_it);
    //             }

    //             if (top_assignments.find(*rhs_it) != top_assignments.end())
    //             {
    //                 *rhs_it = top_assignments.at(*rhs_it);
    //             }

    //             m_nets_to_merge[*rhs_it].push_back(*lhs_it);
    //         }
    //     }

    //     // TODO

    //     // now create all instances of the top entity
    //     // this will recursively instantiate all sub-entities
    //     // if (instantiate(e, nullptr, top_assignments) == nullptr)
    //     // {
    //     //     return false;
    //     // }

    //     return m;
    // }
    // std::shared_ptr<module> instantiate(const instance& inst, std::shared_ptr<module> parent)
    // {
    //     std::map<std::string, std::string> signal_suffixes;
    //     std::map<std::string, std::string> instance_suffixes;

    //     auto& e = m_entities[inst._type];
    //     UNUSED(e);    // TODO remove

    //     std::shared_ptr<module> m = m_netlist->create_module(inst._name, parent);
    //     if (m == nullptr)
    //     {
    //         log_error("hdl_parser", "could not instantiate the module '{}'.", inst._name);
    //         return nullptr;
    //     }

    //     // TODO first move into top_module handler
    //     //     // cache global vcc/gnd types
    //     //     auto vcc_gate_types = m_netlist->get_gate_library()->get_vcc_gate_types();
    //     //     auto gnd_gate_types = m_netlist->get_gate_library()->get_gnd_gate_types();

    //     //     // process instances i.e. gates or other entities
    //     //     for (const auto& inst : e.instances)
    //     //     {
    //     //         // will later hold either module or gate, so attributes can be assigned properly
    //     //         data_container* container;

    //     //         // assign actual signal names to ports
    //     //         std::unordered_map<std::string, std::string> instance_assignments;
    //     //         for (const auto& [pin, signal] : inst.ports)
    //     //         {
    //     //             auto it2 = parent_module_assignments.find(signal);
    //     //             if (it2 != parent_module_assignments.end())
    //     //             {
    //     //                 instance_assignments[pin] = it2->second;
    //     //             }
    //     //             else
    //     //             {
    //     //                 auto it3 = aliases.find(signal);
    //     //                 if (it3 != aliases.end())
    //     //                 {
    //     //                     instance_assignments[pin] = it3->second;
    //     //                 }
    //     //                 else
    //     //                 {
    //     //                     instance_assignments[pin] = signal;
    //     //                 }
    //     //             }
    //     //         }

    //     //         // if the instance is another entity, recursively instantiate it
    //     //         auto entity_it = m_entities.find(inst.type);
    //     //         if (entity_it != m_entities.end())
    //     //         {
    //     //             container = instantiate(entity_it->second, module, instance_assignments).get();
    //     //             if (container == nullptr)
    //     //             {
    //     //                 return nullptr;
    //     //             }
    //     //         }
    //     //         // otherwise it has to be an element from the gate library
    //     //         else
    //     //         {
    //     //             // create the new gate
    //     //             aliases[inst.name] = get_unique_alias(inst.name);

    //     //             std::shared_ptr<gate> new_gate;
    //     //             {
    //     //                 auto gate_types = m_netlist->get_gate_library()->get_gate_types();
    //     //                 auto it         = std::find_if(gate_types.begin(), gate_types.end(), [&](auto& v) { return core_utils::equals_ignore_case(v.first, inst.type); });
    //     //                 if (it == gate_types.end())
    //     //                 {
    //     //                     log_error("hdl_parser", "could not find gate type '{}' in gate library", inst.type);
    //     //                     return nullptr;
    //     //                 }
    //     //                 new_gate = m_netlist->create_gate(it->second, aliases[inst.name]);
    //     //             }

    //     //             if (new_gate == nullptr)
    //     //             {
    //     //                 log_error("hdl_parser", "could not instantiate gate '{}'", inst.name);
    //     //                 return nullptr;
    //     //             }
    //     //             module->assign_gate(new_gate);
    //     //             container = new_gate.get();

    //     //             // if gate is a global type, register it as such
    //     //             if (vcc_gate_types.find(inst.type) != vcc_gate_types.end() && !new_gate->mark_vcc_gate())
    //     //             {
    //     //                 return nullptr;
    //     //             }
    //     //             if (gnd_gate_types.find(inst.type) != gnd_gate_types.end() && !new_gate->mark_gnd_gate())
    //     //             {
    //     //                 return nullptr;
    //     //             }

    //     //             // cache pin types
    //     //             auto input_pins  = new_gate->get_input_pins();
    //     //             auto output_pins = new_gate->get_output_pins();

    //     //             // check for port
    //     //             for (auto [pin_it, net_name] : inst.ports)
    //     //             {
    //     //                 std::string pin = pin_it;    // copy to be able to overwrite

    //     //                 // apply port assignments
    //     //                 {
    //     //                     auto it = instance_assignments.find(pin);
    //     //                     if (it != instance_assignments.end())
    //     //                     {
    //     //                         net_name = it->second;
    //     //                     }
    //     //                 }

    //     //                 // if the net is an internal signal, use its alias
    //     //                 if (std::find(e.signals.begin(), e.signals.end(), net_name) != e.signals.end())
    //     //                 {
    //     //                     net_name = aliases.at(net_name);
    //     //                 }

    //     //                 // get the respective net for the assignment
    //     //                 std::shared_ptr<net> current_net = nullptr;
    //     //                 {
    //     //                     auto it = m_net_by_name.find(net_name);
    //     //                     if (it == m_net_by_name.end())
    //     //                     {
    //     //                         log_warning("hdl_parser", "creating undeclared signal '{}' assigned to port '{}' of instance '{}' (starting at line {})", net_name, pin, inst.name, inst.line_number);

    //     //                         current_net                            = m_netlist->create_net(net_name);
    //     //                         m_net_by_name[current_net->get_name()] = current_net;
    //     //                     }
    //     //                     else
    //     //                     {
    //     //                         current_net = it->second;
    //     //                     }
    //     //                 }

    //     //                 // add net src/dst by pin types
    //     //                 bool is_input = false;
    //     //                 {
    //     //                     auto it = std::find_if(input_pins.begin(), input_pins.end(), [&](auto& s) { return core_utils::equals_ignore_case(s, pin); });
    //     //                     if (it != input_pins.end())
    //     //                     {
    //     //                         is_input = true;
    //     //                         pin      = *it;
    //     //                     }
    //     //                 }
    //     //                 bool is_output = false;
    //     //                 {
    //     //                     auto it = std::find_if(output_pins.begin(), output_pins.end(), [&](auto& s) { return core_utils::equals_ignore_case(s, pin); });
    //     //                     if (it != output_pins.end())
    //     //                     {
    //     //                         is_output = true;
    //     //                         pin       = *it;
    //     //                     }
    //     //                 }

    //     //                 if (!is_input && !is_output)
    //     //                 {
    //     //                     log_error("hdl_parser", "gate '{}' ({}) has no pin '{}'", new_gate->get_name(), new_gate->get_type()->get_name(), pin);
    //     //                     log_error("hdl_parser", "  available input pins: {}", core_utils::join(", ", new_gate->get_type()->get_input_pins()));
    //     //                     log_error("hdl_parser", "  available output pins: {}", core_utils::join(", ", new_gate->get_type()->get_output_pins()));
    //     //                     return nullptr;
    //     //                 }

    //     //                 if (is_output)
    //     //                 {
    //     //                     if (current_net->get_src().gate != nullptr)
    //     //                     {
    //     //                         auto src = current_net->get_src().gate;
    //     //                         log_error("hdl_parser",
    //     //                                   "net '{}' already has source gate '{}' (type {}), cannot assign '{}' (type {})",
    //     //                                   current_net->get_name(),
    //     //                                   src->get_name(),
    //     //                                   src->get_type()->get_name(),
    //     //                                   new_gate->get_name(),
    //     //                                   new_gate->get_type()->get_name());
    //     //                     }
    //     //                     if (!current_net->set_src(new_gate, pin))
    //     //                     {
    //     //                         return nullptr;
    //     //                     }
    //     //                 }

    //     //                 if (is_input && !current_net->add_dst(new_gate, pin))
    //     //                 {
    //     //                     return nullptr;
    //     //                 }
    //     //             }
    //     //         }

    //     //         // assign instance attributes
    //     //         {
    //     //             auto attribute_it = e.instance_attributes.find(inst.name);
    //     //             if (attribute_it != e.instance_attributes.end())
    //     //             {
    //     //                 for (const auto& attr : attribute_it->second)
    //     //                 {
    //     //                     if (!container->set_data("vhdl_attribute", std::get<0>(attr), std::get<1>(attr), std::get<2>(attr)))
    //     //                     {
    //     //                         log_error("hdl_parser", "couldn't set data: key: {}, value_data_type: {}, value: {}", std::get<0>(attr), std::get<1>(attr), std::get<2>(attr));
    //     //                     }
    //     //                 }
    //     //             }
    //     //         }

    //     //         // process generics
    //     //         for (auto [name, value] : inst.generics)
    //     //         {
    //     //             auto bit_vector_candidate = core_utils::trim(core_utils::replace(value, "_", ""));

    //     //             // determine data type
    //     //             auto data_type = std::string();
    //     //             if ((core_utils::to_lower(value) == "true") || (core_utils::to_lower(value) == "false"))
    //     //             {
    //     //                 data_type = "boolean";
    //     //             }
    //     //             else if (core_utils::is_integer(value))
    //     //             {
    //     //                 data_type = "integer";
    //     //             }
    //     //             else if (core_utils::is_floating_point(value))
    //     //             {
    //     //                 data_type = "floating_point";
    //     //             }
    //     //             else if (core_utils::ends_with(value, "s", true) || core_utils::ends_with(value, "sec", true) || core_utils::ends_with(value, "min", true) || core_utils::ends_with(value, "hr", true))
    //     //             {
    //     //                 data_type = "time";
    //     //             }
    //     //             else if ((value[0] == '\"' && value.back() == '\"') && !std::all_of(bit_vector_candidate.begin(), bit_vector_candidate.end(), ::isxdigit))
    //     //             {
    //     //                 value     = value.substr(1, value.size() - 2);
    //     //                 data_type = "string";
    //     //             }
    //     //             else if (value[0] == '\'' && value.back() == '\'')
    //     //             {
    //     //                 value     = value.substr(1, value.size() - 2);
    //     //                 data_type = "bit_value";
    //     //             }
    //     //             else if ((value[0] == 'D' || value[0] == 'X' || value[0] == 'B' || value[0] == 'O') && value[1] == '\"' && value.back() == '\"')
    //     //             {
    //     //                 value     = get_hex_from_number_literal(value);
    //     //                 data_type = "bit_vector";
    //     //             }
    //     //             else
    //     //             {
    //     //                 log_error("hdl_parser", "cannot identify data type of generic map value '{}' in instance '{}'", value, inst.name);
    //     //                 return nullptr;
    //     //             }

    //     //             // store generic information on gate
    //     //             if (!container->set_data("generic", name, data_type, value))
    //     //             {
    //     //                 log_error("hdl_parser", "couldn't set data", value, inst.name);
    //     //                 return nullptr;
    //     //             }
    //     //         }
    //     //     }

    //     //     return module;
    //     return nullptr;
    // }

    // T get_unique_signal_suffix(const T& name)
    // {
    //     // if the name only appears once, we don't have to alias it
    //     if (m_signal_name_occurrences[name] < 2)
    //     {
    //         return "";
    //     }

    //     m_current_signal_index[name]++;

    //     // otherwise, add a unique string to the name
    //     return "__" + std::to_string(m_current_signal_index[name]) + "__";
    // }

    // T get_unique_instance_suffix(const T& name)
    // {
    //     // if the name only appears once, we don't have to alias it
    //     if (m_instance_name_occurrences[name] < 2)
    //     {
    //         return "";
    //     }

    //     m_current_instance_index[name]++;

    //     // otherwise, add a unique string to the name
    //     return "__" + std::to_string(m_current_instance_index[name]) + "__";
    // }

    // void expand_signal_recursively(std::vector<T>& expanded_signal, T current_signal, const std::vector<std::vector<u32>>& ranges, u32 dimension)
    // {
    //     // expand signal recursively
    //     if (bounds.size() > dimension)
    //     {
    //         for (const auto& index : bounds[dimension])
    //         {
    //             this->expand_signal_recursively(expanded_signal, current_signal + "(" + std::to_string(index) + ")", bounds, dimension + 1);
    //         }
    //     }
    //     else
    //     {
    //         // last dimension
    //         expanded_signal.push_back(current_signal);
    //     }
    // }
};

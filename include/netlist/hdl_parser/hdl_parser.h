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
#include <queue>
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
        if (m_netlist == nullptr)
        {
            // error printed in subfunction
            return nullptr;
        }

        // any entities in netlist?
        if (m_entities.empty())
        {
            log_error("hdl_parser", "file did not contain any entities.");
            return nullptr;
        }

        // retrieve available gate types
        std::map<T, std::shared_ptr<const gate_type>> gate_types;
        for (const auto& gt : m_netlist->get_gate_library()->get_gate_types())
        {
            gate_types.emplace(to_special_string(gt.first), gt.second);
        }

        // match each entity's instances to entities and gate types
        for (auto& e : m_entities)
        {
            for (auto& [inst_name, inst] : e.second.get_instances())
            {
                auto& port_assignments = inst.get_port_assignments();

                // instance of entity
                if (auto entity_it = m_entities.find(inst.get_type()); entity_it != m_entities.end())
                {
                    // fill in port width
                    auto& entity_ports = entity_it->second.get_ports();

                    for (auto& [port_name, assignment] : port_assignments)
                    {
                        if (!assignment.first.is_ranges_known())
                        {
                            if (auto port_it = entity_ports.find(port_name); port_it != entity_ports.end())
                            {
                                assignment.first.set_ranges(port_it->second.second.get_ranges());

                                i32 left_size  = assignment.first.get_size();
                                i32 right_size = 0;
                                for (const auto& s : assignment.second)
                                {
                                    right_size += s.get_size();
                                }

                                if (left_size != right_size)
                                {
                                    log_error("hdl_parser",
                                              "port assignment width mismatch: left side has size {} and right side has size {} in line {}.",
                                              left_size,
                                              right_size,
                                              assignment.first.get_line_number());
                                    return nullptr;
                                }
                            }
                            else
                            {
                                log_error("hdl_parser",
                                          "port '{}' is no valid port for instance '{}' of entity '{}' in line {}.",
                                          port_name,
                                          inst_name,
                                          entity_it->first,
                                          assignment.first.get_line_number());
                                return nullptr;
                            }
                        }
                    }
                }
                // instance of gate type
                else if (auto gate_it = gate_types.find(inst.get_type()); gate_it != gate_types.end())
                {
                    std::map<T, std::vector<u32>> pin_groups;
                    for (const auto& pin_group : gate_it->second->get_input_pin_groups())
                    {
                        pin_groups.emplace(to_special_string(pin_group.first), pin_group.second);
                    }
                    for (const auto& pin_group : gate_it->second->get_output_pin_groups())
                    {
                        pin_groups.emplace(to_special_string(pin_group.first), pin_group.second);
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
                                assignment.first.set_ranges({});
                            }
                        }
                        else
                        {
                            log_error("hdl_parser", "pin '{}' is no valid pin for gate '{}' of type '{}' in line {}.", port_name, inst_name, gate_it->first, assignment.first.get_line_number());
                            return nullptr;
                        }

                        i32 left_size  = assignment.first.get_size();
                        i32 right_size = 0;
                        for (const auto& s : assignment.second)
                        {
                            right_size += s.get_size();
                        }

                        if (left_size != right_size)
                        {
                            log_error("hdl_parser",
                                      "port assignment width mismatch: port has width {} and assigned signal has width {} in line {}.",
                                      left_size,
                                      right_size,
                                      assignment.first.get_line_number());
                            return nullptr;
                        }
                    }
                }
                else
                {
                    log_error("hdl_parser", "type '{}' of instance '{}' is neither an entity, nor a gate type in line {}.", inst.get_type(), inst_name, inst.get_line_number());
                    return nullptr;
                }
            }
        }

        // create const 0 and const 1 net, will be removed if unused
        m_zero_net = m_netlist->create_net("'0'");
        if (m_zero_net == nullptr)
        {
            return nullptr;
        }
        m_net_by_name[to_special_string(m_zero_net->get_name())] = m_zero_net;

        m_one_net = m_netlist->create_net("'1'");
        if (m_one_net == nullptr)
        {
            return nullptr;
        }
        m_net_by_name[to_special_string(m_one_net->get_name())] = m_one_net;

        // TODO build the netlist from the intermediate format
        // the last entity in the file is considered the top module
        // if (!build_netlist(m_last_entity))
        // {
        //     return nullptr;
        // }

        // add global GND gate if required by any instance
        if (!m_zero_net->get_destinations().empty())
        {
            auto gnd_type   = m_netlist->get_gate_library()->get_gnd_gate_types().begin()->second;
            auto output_pin = gnd_type->get_output_pins().at(0);
            auto gnd        = m_netlist->create_gate(m_netlist->get_unique_gate_id(), gnd_type, "global_gnd");

            if (!m_netlist->mark_gnd_gate(gnd))
            {
                return nullptr;
            }

            auto gnd_net = m_net_by_name.find("'0'")->second;

            if (!gnd_net->add_source(gnd, output_pin))
            {
                return nullptr;
            }
        }
        else
        {
            m_netlist->delete_net(m_zero_net);
        }

        // add global VCC gate if required by any instance
        if (!m_one_net->get_destinations().empty())
        {
            auto vcc_type   = m_netlist->get_gate_library()->get_vcc_gate_types().begin()->second;
            auto output_pin = vcc_type->get_output_pins().at(0);
            auto vcc        = m_netlist->create_gate(m_netlist->get_unique_gate_id(), vcc_type, "global_vcc");

            if (!m_netlist->mark_vcc_gate(vcc))
            {
                return nullptr;
            }

            auto vcc_net = m_net_by_name.find("'1'")->second;

            if (!vcc_net->add_source(vcc, output_pin))
            {
                return nullptr;
            }
        }
        else
        {
            m_netlist->delete_net(m_one_net);
        }

        for (const auto& net : m_netlist->get_nets())
        {
            bool no_source      = net->get_num_of_sources() == 0 && !net->is_global_input_net();
            bool no_destination = net->get_num_of_destinations() == 0 && !net->is_global_output_net();
            if (no_source && no_destination)
            {
                m_netlist->delete_net(net);
            }
        }

        return m_netlist;
    }

protected:
    class signal
    {
    public:
        signal(u32 line_number, T name, std::vector<std::vector<u32>> ranges = {}, bool binary = false, bool ranges_known = true)
            : _line_number(line_number), _name(name), _ranges(ranges), _binary(binary), _ranges_known(ranges_known)
        {
            compute_size();
        }

        u32 get_line_number() const
        {
            return _line_number;
        }

        const T& get_name() const
        {
            return _name;
        }

        i32 get_size() const
        {
            return _size;
        }

        const std::vector<std::vector<u32>>& get_ranges() const
        {
            return _ranges;
        }

        void set_ranges(std::vector<std::vector<u32>> ranges)
        {
            _ranges       = ranges;
            _ranges_known = true;
            compute_size();
        }

        bool is_binary() const
        {
            return _binary;
        }

        bool is_ranges_known() const
        {
            return _ranges_known;
        }

        bool operator<(const signal& other) const
        {
            // there may be two assignments to the same signal using different bounds
            // without checking bounds, two such signals would be considered equal
            return (_name < other._name) && (_ranges < other._ranges);
        }

    private:
        u32 _line_number;

        // name (may either be the identifier of the signal or a binary string in case of direct assignments)
        T _name;

        // number of bits belonging to the signal (-1 implies "unknown")
        i32 _size;

        // ranges
        std::vector<std::vector<u32>> _ranges;

        // is binary string?
        bool _binary = false;

        // are bounds already known? (should only be unknown for left side of port assignments)
        bool _ranges_known = true;

        void compute_size()
        {
            if (_ranges_known)
            {
                if (_binary)
                {
                    _size = _name.size();
                }
                else if (_ranges.empty())
                {
                    _size = 1;
                }
                else
                {
                    u32 dimension = _ranges.size();
                    _size         = _ranges.at(0).size();

                    for (u32 i = 1; i < dimension; i++)
                    {
                        _size *= _ranges.at(i).size();
                    }
                }
            }
            else
            {
                _size = -1;
            }
        }
    };

    class instance
    {
    public:
        instance(u32 line_number, T type, T name = "") : _line_number(line_number), _type(type), _name(name)
        {
        }

        u32 get_line_number() const
        {
            return _line_number;
        }

        const T& get_type() const
        {
            return _type;
        }

        void set_name(const T& name)
        {
            _name = name;
        }

        const T& get_name() const
        {
            return _name;
        }

        void add_port_assignment(const signal& port, const std::vector<signal>& assignment)
        {
            _port_assignments.emplace(port.get_name(), std::make_pair(port, assignment));
        }

        std::map<T, std::pair<signal, std::vector<signal>>>& get_port_assignments()
        {
            return _port_assignments;
        }

        void add_generic_assignment(const T& generic, const T& data_type, const T& assignment)
        {
            _generic_assignments.emplace(generic, std::make_pair(data_type, assignment));
        }

        const std::map<T, std::pair<T, T>>& get_generic_assignments() const
        {
            return _generic_assignments;
        }

        bool operator<(const instance& other) const
        {
            return _name < other._name;
        }

    private:
        u32 _line_number;

        // type
        T _type;

        // name
        T _name;

        // port assignments: port_name -> (port_signal, assignment_signals)
        std::map<T, std::pair<signal, std::vector<signal>>> _port_assignments;

        // generic assignments: generic_name -> (data_type, data_value)
        std::map<T, std::pair<T, T>> _generic_assignments;
    };

    class entity
    {
    public:
        enum class attribute_target_class
        {
            entity,
            instance,
            signal
        };

        entity(u32 line_number, T name) : _line_number(line_number), _name(name)
        {
        }

        u32 get_line_number() const
        {
            return _line_number;
        }

        const T& get_name() const
        {
            return _name;
        }

        void add_port(const T& direction, const signal& s)
        {
            _ports.emplace(s.get_name(), std::make_pair(direction, s));
        }

        const std::map<T, std::pair<T, signal>>& get_ports() const
        {
            return _ports;
        }

        void add_signal(const signal& s)
        {
            _signals.emplace(s.get_name(), s);
        }

        void add_signals(const std::map<T, signal>& signals)
        {
            _signals.insert(signals.begin(), signals.end());
        }

        const std::map<T, signal>& get_signals() const
        {
            return _signals;
        }

        void add_assignment(const std::vector<signal>& s, const std::vector<signal>& assignment)
        {
            _assignments.emplace(std::make_pair(s, assignment));
        }

        const std::set<std::pair<std::vector<signal>, std::vector<signal>>>& get_assignments() const
        {
            return _assignments;
        }

        void add_instance(const instance& inst)
        {
            _instances.emplace(inst.get_name(), inst);
        }

        std::map<T, instance>& get_instances()
        {
            return _instances;
        }

        void add_attribute(attribute_target_class target_class, const T& target, const T& name, const T& type, const T& value)
        {
            _attributes[target_class][target].emplace(std::make_tuple(name, type, value));
        }

        const std::map<T, std::set<std::tuple<T, T, T>>>& get_attributes(attribute_target_class target_class) const
        {
            return _attributes[target_class];
        }

        void initialize()
        {
            if (_initialized)
            {
                _expanded_ports.clear();
                _expanded_signals.clear();
                _expanded_assignments.clear();
            }

            for (const auto& [port_name, p] : _ports)
            {
                _expanded_ports.emplace(port_name, expand_signal(p.second));
            }

            for (const auto& [signal_name, s] : _signals)
            {
                _expanded_signals.emplace(signal_name, expand_signal(s));
            }

            std::vector<T> expanded_signals;
            std::vector<T> expanded_assignments;

            for (const auto& [signals, assignments] : _assignments)
            {
                for (const auto& s : signals)
                {
                    std::vector<T> expanded;
                    expanded = expand_signal(s);
                    expanded_signals.insert(expanded.begin(), expanded.end());
                }

                for (const auto& s : assignments)
                {
                    std::vector<T> expanded;
                    if (s.is_binary())
                    {
                        expanded = expand_binary_signal(s);
                    }
                    else
                    {
                        expanded = expand_signal(s);
                    }
                    expanded_assignments.insert(expanded.begin(), expanded.end());
                }
            }

            std::transform(expanded_signals.begin(), expanded_signals.end(), expanded_assignments.begin(), std::inserter(_expanded_assignments, _expanded_assignments.end()), [](T s, T a) {
                return std::make_pair(s, a);
            });

            _initialized = true;
        }

        bool is_initialized() const
        {
            return _initialized;
        }

        const std::map<T, std::vector<T>>& get_expanded_ports()
        {
            if (_initialized)
            {
                return _expanded_ports;
            }

            return {};
        }

        const std::map<T, std::vector<T>>& get_expanded_signals()
        {
            if (_initialized)
            {
                return _expanded_signals;
            }

            return {};
        }

        const std::map<T, T>& get_expanded_assignments()
        {
            if (_initialized)
            {
                return _expanded_assignments;
            }

            return {};
        }

        bool operator<(const entity& other) const
        {
            return _name < other._name;
        }

    private:
        u32 _line_number;

        // name
        T _name;

        // ports: port_name -> (direction, signal)
        std::map<T, std::pair<T, signal>> _ports;

        // signals: signal_name -> signal
        std::map<T, signal> _signals;

        // assignments: set(lhs, rhs)
        std::set<std::pair<std::vector<signal>, std::vector<signal>>> _assignments;

        // instances: instance_name -> instance
        std::map<T, instance> _instances;

        // attributes: attribute_target -> set(attribute_name, attribute_type, attribute_value)
        std::map<attribute_target_class, std::map<T, std::set<std::tuple<T, T, T>>>> _attributes;

        bool _initialized = false;

        // expanded ports: port_name -> expanded_ports
        std::map<T, std::vector<T>> _expanded_ports;

        // expanded signals: signal_name -> expanded_signals
        std::map<T, std::vector<T>> _expanded_signals;

        // expanded assignments: expanded_signal_name -> expanded_assignment_name
        std::map<T, T> _expanded_assignments;
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
    std::shared_ptr<net> m_zero_net;
    std::shared_ptr<net> m_one_net;
    std::map<T, std::shared_ptr<net>> m_net_by_name;
    std::map<T, std::vector<T>> m_nets_to_merge;

    std::map<T, std::function<bool(net&)>> mark_global_net_function_map = {{"in", &net::mark_global_input_net}, {"out", &net::mark_global_output_net}};

    bool build_netlist(const T& top_module)
    {
        m_netlist->set_design_name(top_module);
        auto& top_entity = m_entities[top_module];

        std::map<std::string, u32> instantiation_count;

        // preparations for alias: count the occurences of all names
        std::queue<entity*> q;
        q.push(&top_entity);

        // top entity instance will be named after its entity, so take into account for aliases
        m_instance_name_occurrences["top_entity"]++;

        // signals will be named after ports, so take into account for aliases
        for (const auto& port : top_entity.get_ports())
        {
            m_signal_name_occurrences[port.first]++;
        }

        while (!q.empty())
        {
            auto e = q.front();
            q.pop();

            instantiation_count[e->get_name()]++;

            for (const auto& s : e->get_signals())
            {
                m_signal_name_occurrences[s.first]++;
            }

            for (const auto& inst : e->get_instances())
            {
                m_instance_name_occurrences[inst.first]++;

                if (auto it = m_entities.find(inst.second.get_type()); it != m_entities.end())
                {
                    q.push(&(it->second));
                }
            }
        }

        // instantiate all entities
        for (auto& e : m_entities)
        {
            e.instantiate();

            if (instantiation_count[e.first] == 0)
            {
                log_warning("hdl_parser", "entity '{}' defined but not used", e.first);
            }
        }

        // for the top module, generate global i/o signals for all ports
        std::unordered_map<T, T> top_assignments;
        auto& expanded_ports = top_entity.get_expanded_ports();

        for (const auto& [port_name, port] : top_entity.get_ports())
        {
            auto direction = port.first;

            for (const auto& expanded_name : expanded_ports.at(port_name))
            {
                auto new_net                 = m_netlist->create_net(to_std_string(expanded_name));
                m_net_by_name[expanded_name] = new_net;

                // for instances, point the ports to the newly generated signals
                top_assignments[expanded_name] = expanded_name;

                if (new_net == nullptr)
                {
                    log_error("hdl_parser", "could not create new net '{}'", expanded_name);
                    return false;
                }

                if (direction == "input" || direction == "inout")
                {
                    if (!new_net->mark_global_input_net())
                    {
                        log_error("hdl_parser", "could not mark net '{}' as global input", expanded_name);
                        return false;
                    }
                }
                if (direction == "output" || direction == "inout")
                {
                    if (!new_net->mark_global_output_net())
                    {
                        log_error("hdl_parser", "could not mark net '{}' as global output", expanded_name);
                        return false;
                    }
                }
            }
        }

        instance top_instance(top_entity.get_line_number(), top_entity.name(), "top_module");
        if (instantiate(top_instance, nullptr, top_assignments) == nullptr)
        {
            log_error("hdl_parser", "could not instantiate module '{}'.", top_entity._name);
            return false;
        }

        // merge nets without gates in between them
        while (!m_nets_to_merge.empty())
        {
            // master = net that other nets are merged into
            // slave = net to merge into master and then delete

            bool progress_made = false;

            for (const auto& [master, merge_set] : m_nets_to_merge)
            {
                // check if none of the slaves is itself a master
                bool is_master = false;
                for (const auto& slave : merge_set)
                {
                    if (m_nets_to_merge.find(slave) != m_nets_to_merge.end())
                    {
                        is_master = true;
                        break;
                    }
                }

                if (is_master)
                {
                    continue;
                }

                auto master_net = m_net_by_name.at(master);
                for (const auto& slave : merge_set)
                {
                    auto slave_net = m_net_by_name.at(slave);

                    // merge sources
                    if (slave_net->is_global_input_net())
                    {
                        master_net->mark_global_input_net();
                    }

                    for (const auto& src : slave_net->get_sources())
                    {
                        slave_net->remove_source(src);

                        if (!master_net->is_a_source(src))
                        {
                            master_net->add_source(src);
                        }
                    }

                    // merge destinations
                    if (slave_net->is_global_output_net())
                    {
                        master_net->mark_global_output_net();
                    }

                    for (const auto& dst : slave_net->get_destinations())
                    {
                        slave_net->remove_destination(dst);

                        if (!master_net->is_a_destination(dst))
                        {
                            master_net->add_destination(dst);
                        }
                    }

                    // merge attributes etc.
                    for (const auto& it : slave_net->get_data())
                    {
                        if (!master_net->set_data(std::get<0>(it.first), std::get<1>(it.first), std::get<0>(it.second), std::get<1>(it.second)))
                        {
                            log_error("hdl_parser", "couldn't set data.");
                        }
                    }

                    // make sure to keep module ports up to date

                    m_netlist->delete_net(slave_net);
                    m_net_by_name.erase(slave);
                }

                m_nets_to_merge.erase(master);
                progress_made = true;
                break;
            }

            if (!progress_made)
            {
                log_error("hdl_parser", "cyclic dependency between signals found, cannot parse netlist.");
                return false;
            }
        }

        return true;
    }

    std::shared_ptr<module> instantiate(const instance& entity_inst, std::shared_ptr<module> parent, std::unordered_map<T, T> parent_module_assignments)
    {
        std::map<T, T> signal_suffixes;
        std::map<T, T> instance_suffixes;

        auto inst_name             = entity_inst.get_name();
        auto inst_type             = entity_inst.get_type();
        const auto& e              = m_entities.at(inst_type);
        const auto& entity_signals = e.get_signals();
        const auto& entity_ports   = e.get_ports();

        instance_suffixes[inst_name] = get_unique_instance_suffix(inst_name);

        std::shared_ptr<module> module;

        if (parent == nullptr)
        {
            module = m_netlist->get_top_module();
            module->set_name(to_std_string(inst_name + instance_suffixes[inst_name]));
        }
        else
        {
            module = m_netlist->create_module(to_std_string(inst_name + instance_suffixes[inst_name]), parent);
        }

        if (module == nullptr)
        {
            return nullptr;
        }

        module->set_type(to_std_string(e.get_name()));

        // create all internal signals
        for (const auto& [signal_name, expanded_signal] : e.get_expanded_signals())
        {
            signal_suffixes[signal_name] = get_unique_signal_suffix(signal_name);

            for (const auto& expanded_name : expanded_signal)
            {
                // create new net for the signal
                auto new_net = m_netlist->create_net(to_std_string(expanded_name + signal_suffixes[expanded_name]));
                if (new_net == nullptr)
                {
                    return nullptr;
                }
                m_net_by_name[expanded_name + signal_suffixes[expanded_name]] = new_net;
            }
        }

        // schedule assigned nets for merging
        for (const auto& [s, assignment] : e.get_expanded_assignments())
        {
            T a = s;
            T b = assignment;

            if (auto it = parent_module_assignments.find(a); it != parent_module_assignments.end())
            {
                a = it->second;
            }
            else
            {
                a = a + signal_suffixes.at(a);
            }

            if (auto it = parent_module_assignments.find(b); it != parent_module_assignments.end())
            {
                b = it->second;
            }
            else if (b != "'0'" && b != "'1'")
            {
                b = b + signal_suffixes.at(b);
            }

            m_nets_to_merge[b].push_back(a);
        }

        // cache global vcc/gnd types
        auto vcc_gate_types = m_netlist->get_gate_library()->get_vcc_gate_types();
        auto gnd_gate_types = m_netlist->get_gate_library()->get_gnd_gate_types();
        auto gate_types     = m_netlist->get_gate_library()->get_gate_types();

        // process instances i.e. gates or other entities
        for (const auto& inst : e.get_instances())
        {
            // will later hold either module or gate, so attributes can be assigned properly
            data_container* container;

            // assign actual signal names to ports
            std::unordered_map<T, T> instance_assignments;

            // expand port assignments
            std::map<T, T> expanded_port_assignments;
            std::vector<T> expanded_ports;
            std::vector<T> expanded_assignments;

            for (const auto& [ports, assignments] : inst.get_port_assignments())
            {
                for (const auto& p : ports)
                {
                    std::vector<T> expanded;
                    expanded = expand_signal(p);
                    expanded_ports.insert(expanded.begin(), expanded.end());
                }

                for (const auto& s : assignments)
                {
                    std::vector<T> expanded;
                    if (s.is_binary())
                    {
                        expanded = expand_binary_signal(s);
                    }
                    else
                    {
                        expanded = expand_signal(s);
                    }
                    expanded_assignments.insert(expanded.begin(), expanded.end());
                }
            }

            std::transform(expanded_ports.begin(), expanded_ports.end(), expanded_assignments.begin(), std::inserter(expanded_port_assignments, expanded_port_assignments.end()), [](T s, T a) {
                return std::make_pair(s, a);
            });

            for (const auto& [expanded_port, expanded_assignment] : expanded_port_assignments)
            {
                if (auto it = parent_module_assignments.find(expanded_assignment); it != parent_module_assignments.end())
                {
                    instance_assignments[expanded_port] = it->second;
                }
                else
                {
                    if (auto suffix_it = signal_suffixes.find(expanded_assignment); suffix_it != signal_suffixes.end())
                    {
                        instance_assignments[expanded_port] = expanded_assignment + suffix_it->second;
                    }
                    else if (expanded_assignment == "'0'" || expanded_assignment == "'1'")
                    {
                        instance_assignments[expanded_port] = expanded_assignment;
                    }
                    else
                    {
                        log_error("hdl_parser", "signal assignment \"{} = {}\" of instance {} is invalid", expanded_port, expanded_assignment, inst.get_name());
                        return nullptr;
                    }
                }
            }

            // if the instance is another entity, recursively instantiate it
            if (m_entities.find(inst.get_type()) != m_entities.end())
            {
                container = instantiate(inst, module, instance_assignments).get();
                if (container == nullptr)
                {
                    return nullptr;
                }
            }

            // TODO finish this
            // // otherwise it has to be an element from the gate library
            // else
            // {
            //     // create the new gate
            //     aliases[inst.name] = get_unique_alias(inst.name);

            //     std::shared_ptr<gate> new_gate;

            //     if (auto gate_type_it = gate_types.find(inst.type); gate_type_it == gate_types.end())
            //     {
            //         log_error("hdl_parser", "could not find gate type '{}' in gate library '{}'", inst.type, m_netlist->get_gate_library()->get_name());
            //         return nullptr;
            //     }
            //     else
            //     {
            //         new_gate = m_netlist->create_gate(gate_type_it->second, aliases[inst.name]);
            //     }

            //     if (new_gate == nullptr)
            //     {
            //         return nullptr;
            //     }

            //     module->assign_gate(new_gate);
            //     container = new_gate.get();

            //     // if gate is a global type, register it as such
            //     if (vcc_gate_types.find(inst.type) != vcc_gate_types.end() && !new_gate->mark_vcc_gate())
            //     {
            //         return nullptr;
            //     }
            //     if (gnd_gate_types.find(inst.type) != gnd_gate_types.end() && !new_gate->mark_gnd_gate())
            //     {
            //         return nullptr;
            //     }

            //     // cache pin types
            //     auto input_pins  = new_gate->get_input_pins();
            //     auto output_pins = new_gate->get_output_pins();

            //     // check for port
            //     for (auto [pin, net_name] : inst.ports)
            //     {
            //         // apply port assignments
            //         if (auto instance_it = instance_assignments.find(pin); instance_it != instance_assignments.end())
            //         {
            //             net_name = instance_it->second;
            //         }

            //         // if the net is an internal signal, use its alias
            //         if (std::find(e.signals_expanded.begin(), e.signals_expanded.end(), net_name) != e.signals_expanded.end())
            //         {
            //             net_name = aliases.at(net_name);
            //         }

            //         // get the respective net for the assignment
            //         if (auto net_it = m_net_by_name.find(net_name); net_it == m_net_by_name.end())
            //         {
            //             log_error("hdl_parser", "signal '{}' of {} was not previously declared", net_name, e.name);
            //             return nullptr;
            //         }
            //         else
            //         {
            //             auto current_net = net_it->second;

            //             // add net src/dst by pin types
            //             bool is_input  = std::find(input_pins.begin(), input_pins.end(), pin) != input_pins.end();
            //             bool is_output = std::find(output_pins.begin(), output_pins.end(), pin) != output_pins.end();

            //             if (!is_input && !is_output)
            //             {
            //                 log_error("hdl_parser", "undefined pin '{}' for '{}' ({})", pin, new_gate->get_name(), new_gate->get_type()->get_name());
            //                 return nullptr;
            //             }

            //             if (is_output && !current_net->add_source(new_gate, pin))
            //             {
            //                 return nullptr;
            //             }

            //             if (is_input && !current_net->add_destination(new_gate, pin))
            //             {
            //                 return nullptr;
            //             }
            //         }
            //     }
            // }

            // // process generics
            // for (auto [name, value] : inst.generics)
            // {
            //     auto bit_vector_candidate = core_utils::trim(core_utils::replace(value, "_", ""));

            //     // determine data type
            //     auto data_type = std::string();

            //     if (core_utils::is_integer(value))
            //     {
            //         data_type = "integer";
            //     }
            //     else if (core_utils::is_floating_point(value))
            //     {
            //         data_type = "floating_point";
            //     }
            //     else if (core_utils::starts_with(value, "\"") && core_utils::ends_with(value, "\""))
            //     {
            //         value     = value.substr(1, value.size() - 2);
            //         data_type = "string";
            //     }
            //     else if (value.find('\'') != std::string::npos)
            //     {
            //         value     = get_number_from_literal(value, 16);
            //         data_type = "bit_vector";
            //     }
            //     else
            //     {
            //         log_error("hdl_parser", "cannot identify data type of generic map value '{}' in instance '{}'", value, inst.name);
            //         return nullptr;
            //     }

            //     // store generic information on gate
            //     if (!container->set_data("generic", name, data_type, value))
            //     {
            //         log_error("hdl_parser", "couldn't set data", value, inst.name);
            //         return nullptr;
            //     }
            // }
        }

        return module;
    }

    T get_unique_signal_suffix(const T& name)
    {
        // if the name only appears once, we don't have to suffix it
        if (m_signal_name_occurrences[name] < 2)
        {
            return "";
        }

        m_current_signal_index[name]++;

        // otherwise, add a unique string to the name
        return "(" + std::to_string(m_current_signal_index[name]) + ")";
    }

    T get_unique_instance_suffix(const T& name)
    {
        // if the name only appears once, we don't have to alias it
        if (m_instance_name_occurrences[name] < 2)
        {
            return "";
        }

        m_current_instance_index[name]++;

        // otherwise, add a unique string to the name
        return "(" + std::to_string(m_current_instance_index[name]) + ")";
    }

    std::vector<T> expand_signal_vector(const std::vector<signal>& signals, bool allow_binary)
    {
        std::vector<T> res;

        for (const auto& s : signals)
        {
            std::vector<T> expanded;

            if (allow_binary && s.is_binary())
            {
                expanded = expand_binary_signal(s);
            }
            else
            {
                expanded = expand_signal(s);
            }

            res.insert(expanded.begin(), expanded.end());
        }

        return res;
    }

    std::vector<T> expand_signal(const signal& s)
    {
        std::vector<T> res;

        expand_signal_recursively(res, s.get_name(), s.get_ranges(), 0);

        return res;
    }

    std::vector<T> expand_binary_signal(const signal& s)
    {
        std::vector<T> res;

        for (const auto& bin_value : s.get_name())
        {
            res.push_back(T(1, bin_value));
        }

        return res;
    }

    void expand_signal_recursively(std::vector<T>& expanded_signal, const T& current_signal, const std::vector<std::vector<u32>>& ranges, u32 dimension)
    {
        // expand signal recursively
        if (ranges.size() > dimension)
        {
            for (const auto& index : ranges[dimension])
            {
                this->expand_signal_recursively(expanded_signal, current_signal + "(" + std::to_string(index) + ")", ranges, dimension + 1);
            }
        }
        else
        {
            // last dimension
            expanded_signal.push_back(current_signal);
        }
    }

    std::string to_std_string(const T& str)
    {
        return std::string(str.data());
    }

    T to_special_string(const std::string& str)
    {
        return T(str.data());
    }
};

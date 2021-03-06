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

#include "def.h"

#include "netlist/boolean_function.h"
#include "netlist/data_container.h"
#include "netlist/endpoint.h"
#include "netlist/gate_library/gate_type/gate_type.h"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

/* forward declaration */
class netlist;
class net;
class module;
struct endpoint;

/**
 * Gate class containing information about a gate including its location, functions, and module.
 *
 * @ingroup netlist
 */
class NETLIST_API gate : public data_container, public std::enable_shared_from_this<gate>
{
    friend class netlist_internal_manager;

public:
    /**
     * Gets the unique id of the gate.
     *
     * @returns The gate's id.
     */
    u32 get_id() const;

    /**
     * Gets the parent netlist of the gate.
     *
     * @returns The netlist.
     */
    std::shared_ptr<netlist> get_netlist() const;

    /**
     * Gets the gate's name.
     *
     * @returns The name.
     */
    std::string get_name() const;

    /**
     * Sets the gate's name.
     *
     * @param[in] name - The new name.
     */
    void set_name(const std::string& name);

    /**
     * Gets the type of the gate.
     *
     * @returns The gate's type.
     */
    std::shared_ptr<const gate_type> get_type() const;

    /**
     * Checks whether the gate's location in the layout is available.
     *
     * @returns True if valid location data is available, false otherwise.
     */
    bool has_location() const;

    /**
     * Sets the physical location x-coordinate of the gate in the layout.
     * Only positive values are valid, negative values will be regarded as no location assigned.
     *
     * @param[in] x - The gate's x-coordinate.
     */
    void set_location_x(float x);

    /**
     * Gets the physical location x-coordinate of the gate in the layout.
     *
     * @returns The gate's x-coordinate.
     */
    float get_location_x() const;

    /**
     * Sets the physical location y-coordinate of the gate in the layout.
     * Only positive values are valid, negative values will be regarded as no location assigned.
     *
     * @param[in] y - The gate's y-coordinate.
     */
    void set_location_y(float y);

    /**
     * Gets the physical location y-coordinate of the gate in the layout.
     *
     * @returns The gate's y-coordinate.
     */
    float get_location_y() const;

    /**
     * Sets the physical location of the gate in the layout.
     *
     * @param[in] location - A pair <x-coordinate, y-coordinate>.
     */
    void set_location(const std::pair<float, float>& location);

    /**
     * Gets the physical location of the gate in the layout.
     *
     * @returns A pair <x-coordinate, y-coordinate>.
     */
    std::pair<float, float> get_location() const;

    /**
     * Gets the module in which this gate is contained.
     *
     * @returns The module.
     */
    std::shared_ptr<module> get_module() const;

    /**
     * Get the boolean function associated with a specific name.
     * This name can for example be an output pin of the gate or a defined functionality like "reset".
     * If name is empty, the function of the first output pin is returned.
     * If there is no function for the given name, an empty function is returned.
     *
     * @param[in] name - The function name.
     * @returns The boolean function.
     */
    boolean_function get_boolean_function(std::string name = "") const;

    /**
     * Get a map from function name to boolean function for all boolean functions associated with this gate.
     *
     * @param[in] only_custom_functions - If true, this returns only the functions which were set via add_boolean_function.
     * @returns A map from function name to function.
     */
    std::unordered_map<std::string, boolean_function> get_boolean_functions(bool only_custom_functions = false) const;

    /**
     * Add the boolean function with the specified name only for this gate.
     *
     * @param[in] name - The function name, usually an output port.
     * @param[in] func - The function.
     */
    void add_boolean_function(const std::string& name, const boolean_function& func);

    /**
     * Mark this gate as a global vcc gate.
     *
     * @returns True on success.
     */
    bool mark_vcc_gate();

    /**
     * Mark this gate as a global gnd gate.
     *
     * @returns True on success.
     */
    bool mark_gnd_gate();

    /**
     * Unmark this gate as a global vcc gate.
     *
     * @returns True on success.
     */
    bool unmark_vcc_gate();

    /**
     * Unmark this gate as a global gnd gate.
     *
     * @returns True on success.
     */
    bool unmark_gnd_gate();

    /**
     * Checks whether this gate is a global vcc gate.
     *
     * @returns True if the gate is a global vcc gate.
     */
    bool is_vcc_gate() const;

    /**
     * Checks whether this gate is a global gnd gate.
     *
     * @returns True if the gate is a global gnd gate.
     */
    bool is_gnd_gate() const;

    /*
     *      pin specific functions
     */

    /**
     * Get a list of all input pin types of the gate.
     *
     * @returns A vector of input pin types.
     */
    std::vector<std::string> get_input_pins() const;

    /**
     * Get a list of all output pin types of the gate.
     *
     * @returns A vector of output pin types.
     */
    std::vector<std::string> get_output_pins() const;

    /**
     * Get a set of all fan-in nets of the gate, i.e. all nets that are connected to one of the input pins.
     *
     * @returns A set of all connected input nets.
     */
    std::set<std::shared_ptr<net>> get_fan_in_nets() const;

    /**
     * Get the fan-in net which is connected to a specific input pin.
     *
     * @param[in] pin_type - The input pin type.
     * @returns The connected input net.
     */
    std::shared_ptr<net> get_fan_in_net(const std::string& pin_type) const;

    /**
     * Get a set of all fan-out nets of the gate, i.e. all nets that are connected to one of the output pins.
     *
     * @returns A set of all connected output nets.
     */
    std::set<std::shared_ptr<net>> get_fan_out_nets() const;

    /**
     * Get the fan-out net which is connected to a specific output pin.
     *
     * @param[in] pin_type - The output pin type.
     * @returns The connected output net.
     */
    std::shared_ptr<net> get_fan_out_net(const std::string& pin_type) const;

    /**
     * Get a set of all unique predecessor endpoints of the gate.
     * A filter can be supplied which filters out all potential values that return false.
     *
     * @param[in] filter - a function to filter the output. Leave empty for no filtering.
     * @returns A set of unique predecessor endpoints.
     */
    std::set<endpoint> get_unique_predecessors(const std::function<bool(const std::string& starting_pin, const endpoint& ep)>& filter = nullptr) const;

    /**
     * Get a vector of all direct predecessor endpoints of the gate.
     * A filter can be supplied which filters out all potential values that return false.
     *
     * @param[in] filter - a function to filter the output. Leave empty for no filtering.
     * @returns A vector of predecessor endpoints.
     */
    std::vector<endpoint> get_predecessors(const std::function<bool(const std::string& starting_pin, const endpoint& ep)>& filter = nullptr) const;

    /**
     * Get the direct predecessor endpoint of the gate connected to a specific input pin.
     *
     * @param[in] which_pin - the pin of this gate to get the predecessor from.
     * @returns The predecessor endpoint.
     */
    endpoint get_predecessor(const std::string& which_pin) const;

    /**
     * Get a set of all unique successor endpoints of the gate.
     * A filter can be supplied which filters out all potential values that return false.
     *
     * @param[in] filter - a function to filter the output. Leave empty for no filtering.
     * @returns A set of unique successor endpoints.
     */
    std::set<endpoint> get_unique_successors(const std::function<bool(const std::string& starting_pin, const endpoint& ep)>& filter = nullptr) const;

    /**
     * Get a vector of all direct successor endpoints of the gate.
     * A filter can be supplied which filters out all potential values that return false.
     *
     * @param[in] filter - a function to filter the output. Leave empty for no filtering.
     * @returns A vector of successor endpoints.
     */
    std::vector<endpoint> get_successors(const std::function<bool(const std::string& starting_pin, const endpoint& ep)>& filter = nullptr) const;

private:
    gate(std::shared_ptr<netlist> const g, u32 id, std::shared_ptr<const gate_type> gt, const std::string& name, float x, float y);

    gate(const gate&) = delete;               //disable copy-constructor
    gate& operator=(const gate&) = delete;    //disable copy-assignment

    boolean_function get_lut_function(const std::string& pin) const;

    /* pointer to corresponding netlist parent */
    std::shared_ptr<netlist> m_netlist;

    /* id of the gate */
    u32 m_id;

    /* name of the gate */
    std::string m_name;

    /* type of the gate */
    std::shared_ptr<const gate_type> m_type;

    /* location */
    float m_x;
    float m_y;

    /* owning module */
    std::shared_ptr<module> m_module;

    /* connected nets */
    std::map<std::string, std::shared_ptr<net>> m_in_nets;
    std::map<std::string, std::shared_ptr<net>> m_out_nets;

    /* dedicated functions */
    std::map<std::string, boolean_function> m_functions;
};

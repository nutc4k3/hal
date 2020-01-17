import sys
import os

### Automatically searches for the hal_py.so file and adds its path to the environment variables

dirname = os.path.dirname(__file__)
hal_path = os.path.join(dirname, '../../')
hal_py_path = None
hal_base_path = None

for p in os.listdir(hal_path):
    build_path = os.path.join(hal_path, p)
    if not os.path.isfile(build_path):
        if os.path.exists(os.path.join(hal_path, p + '/lib/hal_py.so')):
            hal_base_path = build_path
            hal_py_path = os.path.join(hal_path, p + '/lib/')
            break

if not hal_py_path is None:
    sys.path.append(hal_py_path)
    os.environ["HAL_BASE_PATH"] = str(build_path)
else:
    print("Can't find hal_py.so instance. Did you build it? If it can't be found anyway, make sure its directory is "
          "added to your PYTHONPATH and your HAL_BASE_PATH to your environment variables.")

###
import unittest
import hal_py
import logging
from functools import partial
import time

# Testing the binding of the classes, that are part of the netlist data structure (netlist, gate, net, module) 

'''
NOTE: Every function is only tested once with all possible amounts of inputs,
      but NOT extensively (its already done in the c++ tests)
'''
# NOTE: gate library functions aren't tested yet


def gate_type_filter(gate_type, gate):
    return gate.get_type().get_name() == gate_type


def gate_type_and_name_filter(gate_type, gate_name, gate):
    return gate.get_type().get_name() == gate_type and gate.get_name() == gate_name

def gate_pin_type_filter(pin_type_fixed, pin_type_param, endpoint_param):
    return pin_type_param == pin_type_fixed



class TestCoreUtils(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        logging.basicConfig()

    @classmethod
    def tearDownClass(cls):
        pass

    def setUp(self):
        # Minimal ID for all: netlists, gates, nets, modules, ...
        self.min_id = 3 # <- just to be sure ;)
        self.log = logging.getLogger("LOG")
        self.netlist = hal_py.netlist
        self.g_lib_name = "EXAMPLE_GATE_LIBRARY"
        time.sleep(1)

    def tearDown(self):
        pass

    # ====== HELPER FUNCTIONS =======
    def create_empty_netlist(self):
        nl = hal_py.netlist_factory.create_netlist(self.g_lib_name)
        return nl

    # Gets the first module with the name <name> from a module set <module_set>
    def get_module_by_name (self, module_set, name):
        for m in module_set:
            if m.get_name() == name:
                return m
        return None

    def get_gate_type_by_name(self, name, gl):
        names_to_type = gl.get_gate_types();
        g_type = names_to_type[name]
        # If the gate type isn't found in the gate library
        if g_type is None:
            print("Gate of type '" + name + "' can't be found in the gate library!")
            return None
        else:
            return g_type

    def create_gate_type_filter(self, type_name):
        return partial(gate_type_filter, type_name)

    def create_gate_type_and_name_filter(self, type_name, gate_name):
        return partial(gate_type_and_name_filter, type_name, gate_name)

    def create_gate_pin_type_filter(self, pin_type):
        return partial(gate_pin_type_filter, pin_type)

    # creates an endpoint by gate and pin_type
    def get_endpoint(self, gate, pin_type):
        ep = hal_py.endpoint()
        ep.set_gate(gate)
        ep.set_pin_type(pin_type)
        return ep








# ========= TEST CASES ==========

# -------- netlist tests --------

    # Testing the python binding for function: set_design_name, set_device_name, get_design_name, get_device_name, get_id
    def test_netlist_identifier(self):
        nl = self.create_empty_netlist()
        nl.set_design_name("design_name")
        self.assertEqual(nl.get_design_name(), "design_name")
        nl.set_device_name("device_name")
        self.assertEqual(nl.get_device_name(), "device_name")
        nl.set_id(123)
        self.assertEqual(nl.get_id(), 123)
        nl.set_input_filename(str(hal_py.core_utils.get_binary_directory()) + "/input_filename")
        self.assertEqual(str(nl.get_input_filename()), (str(hal_py.core_utils.get_binary_directory()) + "/input_filename"))
        self.assertEqual(nl.get_gate_library().get_name(), self.g_lib_name)

    # Testing the python binding for functions: create_gate, get_gate_by_id, get_gates, get_gate_by_id, delete_gate, mark_gnd_gate,
    #  mark_vcc_gate, unmark_vcc_gate, unmark_vcc_gate, is_gnd_gate, is_vcc_gate, is_gate_in_netlist
    def test_netlist_create_gate(self):
        # NOTE: float x, float y ?
        # Create a gate (with id)
        nl = self.create_empty_netlist()
        test_gate = nl.create_gate(self.min_id, self.get_gate_type_by_name("INV", nl.gate_library), "test_gate")
        self.assertIsNotNone(test_gate)
        self.assertEqual(len(nl.get_gates(self.create_gate_type_and_name_filter("INV", "test_gate"))), 1)
        self.assertEqual(test_gate.get_id(), self.min_id)
        self.assertIsNotNone(nl.get_gate_by_id(self.min_id))
        self.assertTrue(nl.is_gate_in_netlist(test_gate))

        # Create a gate (without (passed) id)
        test_gate_no_id = nl.create_gate(self.get_gate_type_by_name("INV", nl.gate_library), "test_gate_no_id")
        self.assertIsNotNone(test_gate_no_id)
        self.assertEqual(len(nl.get_gates(self.create_gate_type_and_name_filter("INV", "test_gate_no_id"))), 1)
        self.assertTrue(nl.is_gate_in_netlist(test_gate_no_id))

        # Mark and unmark a gnd_gate
        nl.mark_gnd_gate(test_gate)
        self.assertTrue(nl.is_gnd_gate(test_gate))
        self.assertEqual(len(nl.get_gnd_gates()) , 1)
        nl.unmark_gnd_gate(test_gate)
        self.assertFalse(nl.is_gnd_gate(test_gate))

        # Mark and unmark a vcc_gate
        nl.mark_vcc_gate(test_gate)
        self.assertTrue(nl.is_vcc_gate(test_gate))
        self.assertEqual(len(nl.get_vcc_gates()) , 1)
        nl.unmark_vcc_gate(test_gate)
        self.assertFalse(nl.is_vcc_gate(test_gate))

        # Delete both gates
        nl.delete_gate(test_gate)
        nl.delete_gate(test_gate_no_id)
        self.assertEqual(len(nl.get_gates(self.create_gate_type_and_name_filter("INV", "test_gate"))), 0)
        self.assertEqual(len(nl.get_gates(self.create_gate_type_and_name_filter("INV", "test_gate_no_id"))), 0)
        


    # Testing the python binding for functions: create_module, get_modules, delete_modules, get_module_by_id,
    #                                           get_top_module, is_module_in_netlist
    def test_netlist_create_module(self):
        # Create a module (with id)
        nl = self.create_empty_netlist()
        test_module = nl.create_module(self.min_id, "test_module", nl.get_top_module())
        self.assertIsNotNone(test_module)
        self.assertIsNotNone(nl.get_module_by_id(self.min_id))
        self.assertTrue(nl.is_module_in_netlist(test_module))

        # Create a module (without (passed) id)
        test_module_no_id = nl.create_module("test_module_no_id", nl.get_top_module())
        self.assertIsNotNone(test_module_no_id)
        self.assertIsNotNone(self.get_module_by_name(nl.get_modules(), "test_module_no_id"))

        # Delete both modules
        nl.delete_module(test_module)
        nl.delete_module(test_module_no_id)
        self.assertIsNone(self.get_module_by_name(nl.get_modules(), "test_module_no_id"))
        self.assertIsNone(self.get_module_by_name(nl.get_modules(), "test_module"))

    # Testing the python binding for functions: create_net, get_nets, delete_net, mark_global_input_net, mark_global_output_net,
    #                                           unmark_global_input_net, unmark_global_output_net, is_global_input_net, is_global_output_net,
    #                                           get_numnets, is_net_in_netlist
    def test_netlist_create_net(self):
        nl = self.create_empty_netlist()
        default_net_amt = len(nl.get_nets())

        # Create a net
        test_net = nl.create_net(self.min_id, "test_net")
        self.assertIsNotNone(test_net)
        self.assertIsNotNone(nl.get_net_by_id(self.min_id))
        self.assertEqual(len(nl.get_nets()), default_net_amt+1)
        self.assertTrue(nl.is_net_in_netlist(test_net))

        # Create a net without passing an id
        test_net_no_id = nl.create_net("test_net")
        self.assertIsNotNone(test_net_no_id)
        self.assertIsNotNone(nl.get_net_by_id(self.min_id))
        self.assertEqual(len(nl.get_nets()), default_net_amt+2)

        # Mark and unmark a global_input_net
        nl.mark_global_input_net(test_net)
        self.assertTrue(nl.is_global_input_net(test_net))
        self.assertEqual(len(nl.get_global_input_nets()), 1)
        nl.unmark_global_input_net(test_net)
        self.assertFalse(nl.is_global_input_net(test_net))

        # Mark and unmark a global_output_net
        nl.mark_global_output_net(test_net)
        self.assertTrue(nl.is_global_output_net(test_net))
        self.assertEqual(len(nl.get_global_output_nets()), 1)
        nl.unmark_global_output_net(test_net)
        self.assertFalse(nl.is_global_output_net(test_net))

        # Delete the net
        nl.delete_net(test_net)
        nl.delete_net(test_net_no_id)
        self.assertEqual(len(nl.get_nets()), default_net_amt)

    
    def test_netlist_unique_id(self):
        nl = self.create_empty_netlist()
        self.assertIsNotNone(nl.get_unique_module_id())
        self.assertIsNotNone(nl.get_unique_net_id())
        self.assertIsNotNone(nl.get_unique_gate_id())


# -------- gate tests --------

    # Testing the python binding for functions: add_boolean_function, get_boolean_function, get_fan_in_net,
    #     get_fan_in_nets, get_fan_out_net, get_fan_out_nets, get_id, get_input_pins, get_location, get_location_x,
    #     get_location_y, get_module, get_name, get_netlist, get_output_pins, get_type, has_location, is_gnd_gate,
    #     is_vcc_gate, mark_gnd_gate, mark_vcc_gate, set_location, set_location_x, set_location_y, set_name
    #     unmark_gnd_gate, unmark_vcc_gate
    def test_gate_functions(self):
        nl = self.create_empty_netlist()
        test_gate = nl.create_gate(self.min_id, self.get_gate_type_by_name("INV", nl.gate_library), "test_gate")
        self.assertIsNotNone(nl)
        self.assertIsNotNone(test_gate)

        # Connect nets to the gate
        in_net = nl.create_net(self.min_id, "in_net")
        out_net = nl.create_net(self.min_id+1, "out_net")
        in_net.add_dst(test_gate, "I")
        out_net.set_src(test_gate, "O")

        # Boolean function
        bf = hal_py.boolean_function("A&B")
        test_gate.add_boolean_function("test_bf", bf)
        self.assertEqual(test_gate.get_boolean_function("test_bf"), bf)
        self.assertEqual(len(test_gate.get_boolean_functions()), 2) # (test_bf -> A&B) + (O -> !I)

        # Get connected nets
        self.assertEqual(test_gate.get_fan_in_net("I"), in_net)
        self.assertEqual(len(test_gate.get_fan_in_nets()), 1)
        self.assertEqual(test_gate.get_fan_out_net("O"), out_net)
        self.assertEqual(len(test_gate.get_fan_out_nets()), 1)

        # Location management (Note: small difference after write and read)
        self.assertFalse(test_gate.has_location())
        test_gate.set_location_x(12.0)
        test_gate.set_location_y(34.0)
        self.assertTrue(test_gate.has_location())
        self.assertEqual(test_gate.get_location_x(), 12.0)
        self.assertEqual(test_gate.get_location_y(), 34.0)
        test_gate.set_location((56.0, 78.0))
        self.assertEqual(test_gate.get_location(), (56.0, 78.0))

        # Identifier
        self.assertEqual(test_gate.get_id(), self.min_id)
        test_gate.set_name("new_name")
        self.assertEqual(test_gate.get_name(), "new_name")
        self.assertEqual(test_gate.get_type(), self.get_gate_type_by_name("INV", nl.gate_library))

        # GND/VCC Marking
        self.assertFalse(test_gate.is_gnd_gate())
        self.assertFalse(test_gate.is_vcc_gate())
        test_gate.mark_gnd_gate()
        test_gate.mark_vcc_gate()
        self.assertTrue(test_gate.is_gnd_gate())
        self.assertTrue(test_gate.is_vcc_gate())
        test_gate.unmark_gnd_gate()
        test_gate.unmark_vcc_gate()
        self.assertFalse(test_gate.is_gnd_gate())
        self.assertFalse(test_gate.is_vcc_gate())

        # Pin Information
        self.assertEqual(test_gate.get_input_pins(), ["I"])
        self.assertEqual(test_gate.get_output_pins(), ["O"])

        # Context Information
        self.assertEqual(test_gate.get_module(), nl.get_top_module())
        self.assertEqual(test_gate.get_netlist(), nl)


    # Testing the python binding for functions: get_predecessor, get_predecessors, get_successors,
    #    get_unique_predecessors, get_unique_successors

    # The following netlist is built:
    #
    #
    #          pred_0 =------+--=               .------=
    #                        '--= test_gate =---+------= suc_0
    #          pred_1 =---------=               |
    #                                           '------= suc_1
    #

    def test_gate_adjacent_functions(self):
        nl = self.create_empty_netlist()
        test_gate = nl.create_gate(self.min_id, self.get_gate_type_by_name("AND3", nl.gate_library), "test_gate")
        self.assertIsNotNone(nl)
        self.assertIsNotNone(test_gate)

        # Create some adjacent gates
        suc_0 = nl.create_gate(self.min_id+1, self.get_gate_type_by_name("AND2", nl.gate_library), "suc_0")
        suc_1 = nl.create_gate(self.min_id+2, self.get_gate_type_by_name("INV", nl.gate_library), "suc_1")
        pred_0 = nl.create_gate(self.min_id+3, self.get_gate_type_by_name("INV", nl.gate_library), "pred_0")
        pred_1 = nl.create_gate(self.min_id+4, self.get_gate_type_by_name("BUF", nl.gate_library), "pred_1")

        # Connect nets to the gate
        in_net_0 = nl.create_net(self.min_id, "in_net_0")
        in_net_1 = nl.create_net(self.min_id+1, "in_net_1")
        out_net = nl.create_net(self.min_id+2, "out_net")
        in_net_0.add_dst(test_gate, "I0")
        in_net_0.add_dst(test_gate, "I1")
        in_net_0.set_src(pred_0, "O")
        in_net_1.add_dst(test_gate, "I2")
        in_net_1.set_src(pred_1, "O")

        out_net.set_src(test_gate, "O")
        out_net.add_dst(suc_0, "I0")
        out_net.add_dst(suc_0, "I1")
        out_net.add_dst(suc_1, "I")

        # Test Predecessors
        #create_gate_pin_type_and_endpoint_filter
        self.assertEqual(len(test_gate.get_predecessors()), 3)
        self.assertEqual(test_gate.get_predecessors(), [self.get_endpoint(pred_0, "O"), self.get_endpoint(pred_0, "O"), self.get_endpoint(pred_1, "O")])
        self.assertEqual(test_gate.get_predecessors(self.create_gate_pin_type_filter("I2")), [self.get_endpoint(pred_1, "O")])
        self.assertEqual(test_gate.get_predecessors(self.create_gate_pin_type_filter("unknownPin")), [])
        # Note: Set comparison?
        #self.assertEqual(test_gate.get_unique_predecessors(), {self.get_endpoint(pred_1, "O"), self.get_endpoint(pred_0, "O")})
        self.assertEqual(test_gate.get_predecessor("I0"), self.get_endpoint(pred_0, "O"))

        # Test Predecessors
        self.assertEqual(len(test_gate.get_successors()), 3)
        self.assertEqual(test_gate.get_successors(), [self.get_endpoint(suc_0, "I0"), self.get_endpoint(suc_0, "I1"), self.get_endpoint(suc_1, "I")])
        self.assertEqual(test_gate.get_successors(self.create_gate_pin_type_filter("O")), [self.get_endpoint(suc_0, "I0"), self.get_endpoint(suc_0, "I1"), self.get_endpoint(suc_1, "I")])
        self.assertEqual(test_gate.get_successors(self.create_gate_pin_type_filter("unknownPin")), [])
        # Note: Set comparison?
        #self.assertEqual(test_gate.get_unique_successors(), {self.get_endpoint(suc_0, "I0"), self.get_endpoint(suc_0, "I1"), self.get_endpoint(suc_1, "I")})


    def test_tryout(self):
        nl = self.create_empty_netlist()
        test_gate = nl.create_gate(self.min_id, self.get_gate_type_by_name("INV", nl.gate_library), "test_gate")
        self.assertIsNotNone(nl)
        #help(test_gate)
        #help(hal_py)



if __name__ == '__main__':
    unittest.main()

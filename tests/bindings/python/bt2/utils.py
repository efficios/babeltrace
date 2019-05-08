import bt2

# Run callable `func` in the context of a component's __init__ method.  The
# callable is passed the Component being instantiated.
#
# The value returned by the callable is returned by run_in_component_init.

def run_in_component_init(func):
    class MySink(bt2._UserSinkComponent):
        def __init__(self, params):
            nonlocal res_bound
            res_bound = func(self)

        def _consume(self):
            pass

    g = bt2.Graph()
    res_bound = None
    g.add_component(MySink, 'comp')

    # We deliberately use a different variable for returning the result than
    # the variable bound to the MySink.__init__ context and delete res_bound.
    # The MySink.__init__ context stays alive until the end of the program, so
    # if res_bound were to still point to our result, it would contribute an
    # unexpected reference to the refcount of the result, from the point of view
    # of the user of this function.  It would then affect destruction tests,
    # for example, which want to test what happens when the refcount of a Python
    # object reaches 0.

    res = res_bound
    del res_bound
    return res

# Create an empty trace class with default values.

def get_default_trace_class():
    def f(comp_self):
        return comp_self._create_trace_class()

    return run_in_component_init(f)

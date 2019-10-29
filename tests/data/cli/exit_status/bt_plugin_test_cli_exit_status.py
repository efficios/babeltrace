import bt2
import signal
import os
import time

bt2.register_plugin(__name__, "test_exit_status")


class StatusIter(bt2._UserMessageIterator):
    def __init__(self, config, output_port):
        self.case = output_port.user_data['case']

    def __next__(self):
        if self.case == "STOP":
            raise bt2.Stop()
        if self.case == "INTERRUPTED":
            os.kill(os.getpid(), signal.SIGINT)

            # Wait until the graph is in the interrupted state.
            timeout_s = 10
            for _ in range(timeout_s * 10):
                if self._is_interrupted:
                    raise bt2.TryAgain()

                time.sleep(0.1)

            raise Exception(
                '{} was not interrupted after {} seconds'.format(
                    self.__class__.__name__, timeout_s
                )
            )

        elif self.case == "ERROR":
            raise TypeError("Raising type error")
        else:
            raise ValueError("Invalid parameter")


@bt2.plugin_component_class
class StatusSrc(bt2._UserSourceComponent, message_iterator_class=StatusIter):
    def __init__(self, config, params, obj):
        self._add_output_port("out", {'case': params['case']})

from typing import Tuple, List, Dict, Callable, Union, Optional

class ArrayType:
    def __getitem__(self, type):
        assert not isinstance(type, tuple), 'Only 1 argument may be given to Array[]'
        return Tuple[type, ...]

Array = ArrayType()

# also: None, bool, int, float, str, bytes, object

# alternative syntax?: e.g. (T, U) instead of Tuple[T, U]
# Any? Iterator? NamedTuple?
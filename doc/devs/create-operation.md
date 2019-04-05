
This is developer documentation for creating brand-new operations on the Morphene blockchain.

- (1) Define operation structure in `morphene_operations.hpp`
- (2) Create `FC_REFLECT` definition for the operation struct.
- (3) Implement `validate()` for the operation struct.
- (4) Add operation to `morphene::protocol::operation`
- (5) Define evaluator for the operation.
- (6) Define required authorities for the operation.
- (7) Define unit tests for the operation.
- (8) Modify existing RPC objects for the operation.
- (9) Create new database object(s) / index(es) for the operation
- (10) Use new database object(s) / index(es)
- TODO (11) Define new RPC's / objects / indexes for the operation in the relevant plugin
- TODO (12) Use RPC's to implement `cli_wallet` functionality for the operation
- TODO Example of handling autonomous processing

## Step 1

- (1a) Newly defined operations should include a member `extensions_type extensions;`.
This definition adds a reserved zero byte to the binary serialization, allowing
"opcode space" to add fields in the future.

- (1b) Operations should directly subclass `base_operation` or `virtual_operation`.
Operations should not subclass any other operation class.  `base_operation` is
for operations that are intended to be issued by a user.  `virtual_operation` is
a non-consensus entity and can never be issued by a user; virtual operations
record automatic actions for inclusion in the user's account history (for example
receiving post rewards).

- (1c) Operation type must have a default constructor to be compatible with FC
serialization.  Most operations only implement default constructor.

## Step 2

- (2a) The `FC_REFLECT` statement includes the fully qualified name of the class, and
a bubble list of fields.  `FC_REFLECT` is used for various preprocessor-based
type introspection tasks, including autogeneration of serialization/deserialization
code for binary and JSON formats.

- (2b) If you forget to put a field in `FC_REFLECT` definition, it will compile, but
will have a bug (the field will not be serialized / deserialized).  In production,
this kind of bug causes potentially disastrous state corruption, and is not always
noticeable at first.  Our CI server runs an automated script `check_reflect.py` to
scan the code for forgotten fields, and fails the build if any are found.

## Step 3

- (3a) The `validate()` method uses `FC_ASSERT` macro, which will throw
`fc::exception` if not true.  `validate()` is responsible to check correctness
conditions using the operation struct alone, it has no access to the
chain state database.  Checks which require chain state belong in `do_apply()`,
not `validate()`.

- (3b) Examples of checks that should go in `validate()`:  Strings do not
contain illegal characters; integer values are of correct sign and within
compile-time lower/upper bounds; variable-length elements are within
compile-time minimum/maximum size limits; ordering relationships between
fields (e.g. `field_1 >= field_2`); an asset name is in a compile-time
set of allowed asset names.

- (3c) Examples of checks that cannot go in `validate()` are:  An account
must (or must not) exist; an account must have sufficient balance; a
particular operation or event must (or must not) have occurred in the past;
a particular date / block number / hardfork number has (or has not) occurred;
any reference to the contents of any operation other than `*this`.  All such
checks must go in the evaluator.

## Step 4

- (4a) The file `operations.hpp` defines the `morphene::protocol::operation`
type, which is an `fc::static_variant` with a lengthy parameter list.  (The
`fc::static_variant` implements a
[tagged union type](https://en.wikipedia.org/wiki/Tagged_union) which uses
C++11 [template parameter pack](http://en.cppreference.com/w/cpp/language/parameter_pack)
to specify the list of potential element types).  The order in which
operations are specified is important, adding a new type will cause the
following types to have different integer ID's, which changes binary
serialization and breaks backward compatibility.

- (4b) New non-virtual operations are usually added just before the beginning
of virtual operations.

- (4c) New virtual operations are usually added at the end.

## Step 5

- (5a) You must add `MORPHENE_DEFINE_EVALUATOR` macro in `evaluator.hpp` to
generate some boilerplate code.  The macro is defined `evaluator.hpp`,
most of the generated code is support code required by the framework and
does not affect the operation itself.

- (5b) The actual code to execute the operation is written in a method called
`opname_evaluator::do_apply( const opname_operation& o )`.  This code has
access to the database in the `_db` class member.

- (5c) All new operations, and all new operation functionality, must be gated
by checking `_db.has_hardfork( ... )` for feature constants defined in
`libraries/chain/hardfork.d`.

- (5d) For this example, we will add a boolean field to the `account_object`,
later we will see how to adapt this field to different objects.

- (5e) Methods which get objects from the database return `const` references.
The most general way to retrieve chain state is with
`get< type, index >( index_value )`.

- (5f) To modify an object on the database, you must issue `db.modify()`,
which takes as arguments the `const` reference, and a callback
(usually specified as a [lambda](http://en.cppreference.com/w/cpp/language/lambda) )
which actually modifies the fields.  (The callback mechanism is used because the
`chainbase` blockchain database framework needs to do bookkeeping before and after
any mutation of state.)

- (5g) The evaluator must be registered in `database::initialize_evaluators()`.

## Step 6

- (6a) Required authorities are implemented by
`get_required_active_authorities()`,
`get_required_posting_authorities()`,
`get_required_owner_authorities()` which modify a caller-provided
`flat_set`.  These methods are usually inline.

- (6b) The operation's fields alone must provide enough information
to compute the required authorities.  As a result of this requirement,
when designing new operations, sometimes it becomes necessary to add
a field which explicitly replicates information already available
in the database.

## Step 7

- (7a) Operations should have separate tests:  `validate` test,
`authorities` test and `apply` test.

- (7b) If unit tests pass, but a bug or a spec change is discovered,
in addition to fixing the bug, code to demonstrate the bug should
be added to the unit test.  The unit test should now fail without the
bugfix code, and pass with the bugfix code.

## Step 8

- (8a) Objects in libraries/chain are part of blockchain core, what's used for consensus
- (8b) When adding fields to blockchain core object, should add the same field to RPC objects
to make it available to JSON clients
- (8c) Add field definition and reflection
- (8d) Initialize RPC object field from DB object field in RPC object constructor

## Step 10

- (10a) You may use `database::create()`, `database::modify()` and `database::remove()`
in evaluators or during per-block processing (these methods are actually implemented in
`chainbase`, the `database` class is derived).
- (10b) The `create()` and `modify()` functions take a callback which should fill in the
new field values for the object.
- (10c) The `modify()` and `remove()` functions take a reference to an existing object.
- (10d) To get a reference to an object, you can use `db.get()` to lookup the object
by index value.  For example
`db.get< account_object, by_name >( "alice" );` which throws an exception
if the object does not exist.  In an evaluator, such an exception will cause the operation
(and any transaction or block that contains it) to fail.
- (10e) To get an object that potentially does not exist, you can use `db.find()`, which
is like `db.get()` but it will return a pointer, or `nullptr` if the object does not exist.
- (10f) To iterate over objects in the order of some index, you can call
`auto& idx = get_index< account_index >().indices().get< by_name >()` to get a reference
to the `multi_index_container` index, then use `begin()`, `end()` for a traversal from
the end, or use `lower_bound()`, `upper_bound()` for traversal beginning at an arbitrary
point.  Many examples exist in the code and `multi_index_container` documentation.
Beware of concurrent modification!
- (10g) To pass a composite key to `db.get()`, `db.find()` or any of the above index
iterator functions, use `boost::make_tuple()`.
- (10h) If you have an object and you want to start iterating at its location, use
`idx.iterator_to()`.  This technique is used by RPC methods implementing paginated queries.
- (10i) Object lookup methods will generally return a `const` reference / pointer.  The
iteration tree structures in `multi_index_container` and the undo fucntionality in
`chainbase` implement some "pre-" and "post-" bookkeeping for every modification.  Using
`const` references for lookups allows the compiler to enforce that modification only
occurs through the `db.create()`, `db.modify()` and `db.remove()` methods which do
the proper bookkeeping.  The non-`const` reference which allows modification of the object
only exists inside the body of the callback where the modification occurs.

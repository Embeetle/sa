// Copyright 2018-2023 Johan Cockx
// Taggable objects and tags
// =========================
//
// Tag is a template class that can attach data of a user-defined type to a
// taggable object,  where a taggable object is an instance of a class derived
// from Taggable.
//
// With T a user-defined type supporting default construction and assignment,
// one can write:
//   class B: public Taggable { ... };
//   B* b = new B ...;
//   T t1 = ...;
//   T t2 = ...;
//   Tag<T> xtag;
//   xtag[b] = t1;
//   Tag<T> ytag;
//   ytag[b] = t2;
//   assert(xtag[b] == t1);
//   assert(ytag[b] == t2);
//
// Each Tag<T> attaches zero or one values of a user-defined type T to a
// taggable object.  Two or more tags can attach a value to the same taggable
// object.  Access to the value attached to a given object is fast: the access
// time is independent of the total number of objects,  and even independent
// of the number of objects with a value attached.  It is linear with the number
// of values attached to the object,  but that is expected to be small.
//
// It is allowed to access a tag value that has never been set.  A tag value
// is automatically created when it is accessed for the first time. The value is
// initialized for built-in types (zero),  pointer types (null pointer) and
// class types (default constructor).  It is not initialized for enums!
// Sorry,  but I believe enum initialization cannot be done generically.
// Use some integer type instead if initialization is required.
//
// It is also possible to determine if a tag value has been accessed or not, and
// to iterate over all objects with a tag value for a given tag.  This
// functionality is useful even if the tag value is unused; to efficiently
// support this case, a template specialization for void tag values is provided.
// A Tag<void> is sometimes called a Mark, because it marks objects without
// attaching any other data.  See class definitions below for further details of
// functionality provided.
//
// Implementation detail:  every taggable object maintains a singly linked list
// of tag values.  Deriving from Taggable increases the memory cost of a class
// by the size of a pointer (the root of the linked list).  To access a tag
// value,  the linked list is traversed to locate the tag value for a given tag.
// The most recently added tag value is examined first.
//

#ifndef __base_Tag_h
#define __base_Tag_h

#include "debug.h"
#include "Vector.h"
#include "debug.h"

namespace base {

  class TagBase;
  class TagLink;

  //--------------------------------------------------------------------------
  class Taggable: public Checked, public NoCopy {
    // Base class for taggable object.  A Tag<T> can attach tag values of type T
    // to any object derived from Taggable.

  public:

    // Default constructor exlined to facilitate debugging.  Constructor
    // initializes linked list.
    Taggable();
    
    // Default destructor exlined to facilitate debugging.  Virtual to avoid
    // warning.
    virtual ~Taggable();

  protected:
    friend class TagBase;
    TagLink*& taglink() const { return _taglink; }

    // Root of the linked list of tag values for this object.
    mutable TagLink* _taglink;
  };

  //--------------------------------------------------------------------------
  struct TagLink: public NoCopy
  {
    // Auxiliary class for Tag implementation.  Base class for an element of a
    // linked list of tag values attached to a taggable object.  Contains all
    // members and methods that do not depend on the user-defined type of the
    // tag value.  For a non-void tag, derived classes below add a tag
    // value field.

    // Pointer to the tag for which this link was constructed.
    TagBase* _base;

    // Index in the base's list of tagged objects.
    size_t _index;

    // Pointer to the next link for the same object, or pointer to sentinel at
    // the end of the list.  The sentinel is defined in TagBase.
    TagLink* _next;
  };
  
  //--------------------------------------------------------------------------
  template<typename Data> struct TagLinkWithData: public TagLink
  {
    // Auxiliary class for Tag implementation.  A TagLink with a tag value of a
    // user-defined type 'Data'.  Data must be default-constructible and
    // assignable.  Initial tag value is determined by the default constructor
    // of Data; if Data has no explicit default constructor, it remains
    // uninitialized.  Specialized derived classes defined below implement other
    // initialization strategies.

    Data _data;
    TagLinkWithData() {}
    TagLinkWithData(const Data &data): _data(data) {}
  };

  //--------------------------------------------------------------------------
  template<typename Data, Data initial_value = 0>
  struct TagLinkWithInitializedData: public TagLinkWithData<Data>
  {
    // Auxiliary class for Tag implementation.  A TagLink with a tag value of a
    // user-defined type 'Data' and a user-specified initial value. Data must
    // have a constructor accepting the specified initial value as argument and
    // must be assignable.

    TagLinkWithInitializedData(): TagLinkWithData<Data>(initial_value) { }
  };

  //--------------------------------------------------------------------------
  template<typename Data> struct TagLinkWithZeroInitializedData:
    public TagLinkWithData<Data>
  {
    // Auxiliary class for Tag implementation.  A TagLink with a tag value of a
    // user-defined type 'Data'.  Initial tag value is zero (or a null pointer).
    // Data must have a constructor accepting zero as argument and must be
    // assignable.
    //
    // Implementation note: it is not a good idea to implement
    // TagLinkWithZeroInitializedData as a specialization of
    // TagLinkWithInitializedData, because C++ does not accept '0' as actual
    // value for a non-type template argument of a pointer or occurrence type.
    // The only actual value accepted for such a template argument is the
    // address of (or reference to) an external or exported variable or
    // function.  Even the address of or reference to a static file scope
    // variable or function is not accepted.

    TagLinkWithZeroInitializedData(): TagLinkWithData<Data>(0) {}
  };

  //--------------------------------------------------------------------------
  class TagBase: public NoCopy, public Checked {
    //
    // Auxiliary base class for Tag implementing all methods that do not
    // depend on the user-defined types of the base object or tag value.
    // These methods are not publically accessible from Tag.
      
  public:
    // Default constructor exlined to facilitate debugging.
    TagBase();
    
    // Default destructor exlined to facilitate debugging.  Virtual to avoid
    // warning.
    virtual ~TagBase();

    // Find and return the link for the given object.  Return null if not found.
    TagLink* find_link(const Taggable* taggable) const;

    // Insert and return a new link.
    TagLink* insert_link(Taggable* taggable);

    // Remove and destroy an existing link for the given object.
    void remove_link(Taggable* taggable);

    // Remove and destroy the link for the last object in the list of objects
    // maintained by TagCore (see further). Uses destroy_link_of_last_object
    // instead of destroy_link to obtain a faster implementation that only works
    // for the last object.  Destroying the link for the last object is a common
    // case that warrants specialized code. It is used by the TagCore destructor
    // and clear methods.
    void remove_link_of_last_object(Taggable* taggable);

    // Insert a new link if taggable has no link yet.  Return existing or new
    // link.
    TagLink* include_link(Taggable* taggable);

    // Remove and destroy the link for the given object if it exists.
    void exclude_link(Taggable* taggable);

  protected:

    // Sentinel for linked lists of TagLinks.
    static TagLink _sentinel;

    // Implementation choice: most of the code in this class to
    // find/add/remove links is independent of the user-defined type of the
    // tag value.  The exceptions are creation and destruction of the link
    // object.  There are two implementation options:  either all the code
    // that directly or indirectly creates or destroys a link is moved to a
    // derived template class and duplicated for each user-defined type,  or
    // the code is kept in the non-template base class and virtual methods are
    // used to create and destroy links.  Considering that the main
    // disadvantage of the second option is a small runtime overhead when
    // calling the virtual functions,  but that this is largely offset by the
    // advantage of a smaller code size that has a better chance of fitting in
    // the code cache,  the second option (with virtual functions) has been
    // selected.
    //
    // Virtual functions to create and destroy links.
    virtual TagLink* create_link(Taggable* taggable) = 0;
    virtual void destroy_link(TagLink* link) = 0;
    virtual void destroy_link_of_last_object(TagLink* link) = 0;
  };
  
  //--------------------------------------------------------------------------
  template <class Link, class Object> class TagCore: protected TagBase {
    //
    // Base class for tag implementing all methods that are common to void and
    // non-void tags.  The methods publicly defined here are accessible from any
    // tag.
    //
    // Maintains a list of tagged objects.  Objects can be of any class derived
    // from Taggable, and the list is maintained as a list of objects, not
    // taggables, and is made available as such to the client.  The list is
    // treated like a set: the order of the entries is not guaranteed.
    //

  public:

    // Remove all tag values and destroy the tag.
    // Destructor is virtual to avoid compiler warning in the presence of
    // virtual methods.
    virtual ~TagCore() { clear(); }

    // Return true iff the object has already been tagged.
    bool done(const Object* object) const {
      assert(object);
      return find_link(object)!=0;
    }
    bool operator()(const Object* object) const { return done(object); }
    bool includes(const Object* object) const { return done(object); }
    

    // Return the index of an object in the tag. Precondition: the object has
    // been tagged.
    size_t index_of(const Object* object) const
    {
      assert(done(object));
      return find_link(object)->_index;
    }

    // Create a tag for the object if it does not have a tag yet.
    void set_at(Object* object)
    {
      assert(object);
      include_link(object);
    }
    void set_done(Object* object) { set_at(object); }
    void include(Object* object) { set_at(object); }

    // Remove the tag for the object if there is one.
    void clear_at(Object* object)
    {
      assert(object);
      exclude_link(object);
    }
    void exclude(Object* object) { clear_at(object); }

    // Remove tag from all tagged objects.
    void clear() { while (!empty()) pop_back(); }

    // Return the set of tagged objects.
    const Vector<Object*>& objects() const { return _objects; }

    // Return number of tagged objects.
    size_t size() const { return _objects.size(); }

    // Return the i'th tagged object, with i = 0..size()-1.
    Object *operator[](size_t i) const { return at(i); }
    Object *at(size_t i) const { return _objects[i]; }

    // Return true iff no objects are tagged.
    // Equivalent to but potentially more efficient than size() == 0.
    bool empty() const { return _objects.empty(); }

    // Return the last tagged object in the list.
    Object *back() const { return _objects.back(); }

    // Remove tag from the last tagged object in the list.
    void pop_back() { remove_link(_objects.back()); }

    // Remove tag from the last tagged object in the list and return that
    // object.
    Object *pop()
    {
      Object *object = back();
      pop_back();
      return object;
    }

    // Add object to tag if it does not have a tag yet.
    void push(Object *object) { set_at(object); }

    // Return true if all tagged objects of 'tag' are also tagged by '*this'.
    template <class OtherLink>
    bool includes(const TagCore<OtherLink,Object>& tag)
    {
      // TODO: more efficient implementation
      return _objects.includes(tag._objects);
    }

    // Return true if no tagged objects of 'tag' are also tagged by '*this'.
    template <class OtherLink>
    bool excludes(const TagCore<OtherLink,Object>& tag)
    {
      // TODO: more efficient implementation
      return _objects.excludes(tag._objects);
    }

  protected:

    // Set of objects with tag value.
    Vector<Object*> _objects;

    // When a link is created, the corresponding object is also inserted in
    // the list of tagged objects.
    TagLink* create_link(Taggable* taggable) {
      Link *link = new Link;
      link->_index = _objects.size();
      // Note: the cast below can be static, because Tag should always be
      // instantiated with an Object class derived from Taggable
      _objects.push_back(static_cast<Object*>(taggable));
      return link;
    }

    // When a link is destroyed, the corresponding object is also removed from
    // the list of tagged objects.  To make sure that destruction is a constant
    // time operation, independent of the number of tagged objects, the object
    // is replaced by the last object in the list, and the index fields are
    // modified accordingly.
    void destroy_link(TagLink* link) {
      // If the link's object is the last object for this tag, the general code
      // will not work, because the link has already been removed from the
      // linked list: the find_link call below returns a null pointer. Anyway, a
      // simpler solution is possible in that case.
      if (link->_index == _objects.size()-1) {
        _objects.pop_back();
      } else {
        find_link(_objects.back())->_index = link->_index;
        _objects.remove_at(link->_index);
      }
      delete static_cast<Link*>(link);
    }
    
    void destroy_link_of_last_object(TagLink* link) {
      assert(link->_index == _objects.size()-1);
      _objects.pop_back();
      delete static_cast<Link*>(link);
    }
  };
    
  //--------------------------------------------------------------------------
  template <typename Data, class Link, class Object>
  class TagCoreWithData: public TagCore<Link,Object> {
    //
    // Base class for Tag with tag values of user-defined type Data.
    // Implements methods to access tag values.  Other public methods are
    // inherited from TagCore.
    //
  public:

    // Return a reference to the tag value for a given object.
    // If no tag value exists yet,  create it first.
    Data& operator[](Object* object) { return at(object); }
    Data& at(Object* object) {
      return static_cast<Link*>(this->include_link(object))->_data;
    }

    // Return a reference to the tag value for a given object.
    // If no tag value exists yet,  an assertion fails.
    Data& operator[](Object* object) const { return at(object); }
    Data& at(Object* object) const {
      Link *link = static_cast<Link*>(this->find_link(object));
      assert(link);
      return link->_data;
    }

    // Return the i'th tagged object, with i = 0..size()-1.
    // Also defined in TagCore,  but must be redefined here due to overloading.
    Object *operator[](size_t i) const { return at(i); }
    Object *at(size_t i) const { return TagCore<Link,Object>::at(i); }

    // Set the tag value for a given object.
    // If no tag value exists yet,  create it first.
    void set_at(Object* object, const Data& data) { at(object) = data; }
  };

  //--------------------------------------------------------------------------
  template <typename Data, class Object = Taggable>
  class Tag: public TagCoreWithData<Data,TagLinkWithData<Data>,Object> {
    //
    // Tag<Data> can attach a tag value of type Data to any object of type
    // Object,  which must be equal to or derived from Taggable.
    //
    // All public methods of Tag are inherited from TagCoreWithData and TagCore.
    //
    // Values of type Data must be default constructible.
    // Specializations of this template for zero-initialized or void tag
    // values are defined below.
    //
  };

  //--------------------------------------------------------------------------
  template <class Object>
  class Tag<void,Object>: public TagCore<TagLink,Object> {
    //
    // Specialization of Tag for void tag values.  Void tag values can be
    // useful because they mark an object.  One use is marking nodes done
    // during depth first traversal of a graph.  A void tag is sometimes
    // called a Mark.
    //
    // All public methods are inherited from TagCore.
    //
  };
  typedef Tag<void> Mark;

  //--------------------------------------------------------------------------
  template <typename Data, class Object, Data init>
  class InitializedTag:
    public TagCoreWithData<Data,TagLinkWithInitializedData<Data,init>,Object>
  {
    //
    // Specialization of Tag for initialized tag values with user-specified
    // initial value.  Useful for enum types and to create tags with
    // user-defined data types that have no or an inappropriate default
    // constructor.  All public methods are inherited from TagCoreWithData and
    // TagCore.
    //
    // To ensure that all tag values for tags with data type E are initialized
    // to e, use the following specialization of Tag:
    //
    //   template <class Object>
    //   class Tag<E,Object>: public InitializedTag<E,Object,e> {};
    //
  };

  //--------------------------------------------------------------------------
  template <typename Data, class Object>
  class ZeroInitializedTag:
    public TagCoreWithData<Data,TagLinkWithZeroInitializedData<Data>,Object>
  {
    //
    // Specialization of Tag for zero-initialized tag values.  Useful for
    // built-in types such as int that are otherwise not initialized.
    // All public methods are inherited from TagCoreWithData and TagCore.
    // 
  };

  //--------------------------------------------------------------------------
  // Specializations for built-in types ensure initialization of tag values
  // to zero.  This does not work for enums.
  //
  template<class Object> class Tag<bool,Object>:
    public ZeroInitializedTag<bool,Object>
  {};
  template<class Object> class Tag<char,Object>:
    public ZeroInitializedTag<char,Object>
  {};
  template<class Object> class Tag<unsigned char,Object>:
    public ZeroInitializedTag<unsigned char,Object>
  {};
  template<class Object> class Tag<short,Object>:
    public ZeroInitializedTag<short,Object>
  {};
  template<class Object> class Tag<unsigned short,Object>:
    public ZeroInitializedTag<unsigned short,Object>
  {};
  template<class Object> class Tag<int,Object>:
    public ZeroInitializedTag<int,Object>
  {};
  template<class Object> class Tag<unsigned int,Object>:
    public ZeroInitializedTag<unsigned int,Object>
  {};
  template<class Object> class Tag<long,Object>:
    public ZeroInitializedTag<long,Object>
  {};
  template<class Object> class Tag<unsigned long,Object>:
    public ZeroInitializedTag<unsigned long,Object>
  {};
  template<class Object> class Tag<float,Object>:
    public ZeroInitializedTag<float,Object>
  {};
  template<class Object> class Tag<double,Object>:
    public ZeroInitializedTag<double,Object>
  {};
  template<typename Data, class Object> class Tag<Data*,Object>:
    public ZeroInitializedTag<Data*,Object>
  {};
}

#endif


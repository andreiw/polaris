/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 *ident	"@(#)Configuration.java	1.4	05/11/30 SMI"
 *
 */

package com.sun.solaris.service.pools;

import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;

/**
 * The <code>Configuration</code> class represents a pools configuration.
 */
public class Configuration extends Element
{
	/**
	 * Indicates whether the configuration represents a usable
	 * configuration.
	 */
	private boolean _valid = false;

	/**
	 * A reference to the native libpool object represented by
	 * this instance.
	 */
	private long _this;

	/**
	 * The name of this instance.
	 */
	private String name;

	/**
	 * The cache of elements known to this configuration.
	 */
	private Map elements;

	/**
	 * The key of the configuration.
	 */
	private String key;

	/**
	 * Constructor
	 * @param location The location of the configuration.
	 * @param perms The OR'd permissions used when opening this
	 * configuration.
	 * @exception PoolsException The error message generated by
	 * libpool.
	 */
	public Configuration(String location, int perms) throws PoolsException
	{
		if (((_this = PoolInternal.pool_conf_alloc())) == 0)
			throw new PoolsException();
		_conf = this;
		open(location, perms);
		elements = new HashMap();
	}

        /**
         * Reclaim the memory allocated for this configuration by the C
	 * proxy.
         *
         * @throws Throwable If freeing this configuration fails.
         */
	protected void finalize() throws Throwable
	{
		try
		{
			close();
			if (_this != 0) {
				PoolInternal.pool_conf_free(_this);
				_this = 0;
			}
		}
		finally
		{
			super.finalize();
		}
	}

	/**
	 * Returns a pointer to the native configuration, wrapped by this
	 * instance.
	 *
	 * @return the configuration pointer.
	 */
	final long getConf()
	{
		return (_this);
	}

	/**
	 * Opens the configuration at the supplied location and with the
	 * supplied permissions.
	 *
	 * @throws PoolsException if there is an error opening the
	 * configuration.
	 */
	public void open(String location, int perms) throws PoolsException
	{
		if (_valid == false) {
			if (PoolInternal.pool_conf_open(getConf(), location,
				perms) != PoolInternal.PO_SUCCESS) {
				throw new PoolsException();
			}
			_valid = true;
			name = getStringProperty("system.name");
			key = "system." + name;
		}
	}

	/**
	 * Closes the configuration.
	 *
	 */
	public void close()
	{
		if (_valid == true) {
			elements.clear();
			PoolInternal.pool_conf_close(getConf());
			name = key = null;
		}
		_valid = false;
	}

	/**
	 * Returns the location of the configuration.
	 *
	 * @return the location of the configuration.
	 */
	public String getLocation()
	{
		return (PoolInternal.pool_conf_location(getConf()));
	}

	/**
	 * Returns the status of the configuration.
	 *
	 * @return the status of the configuration.
	 */
	public int status()
	{
		return (PoolInternal.pool_conf_status(getConf()));
	}

	/**
	 * Remove the configuration.
	 *
	 * @throws PoolsExecption If the removal fails.
	 */
	public void remove() throws PoolsException
	{
		if (PoolInternal.pool_conf_remove(getConf()) !=
		    PoolInternal.PO_SUCCESS)
			throw new PoolsException();
	}

	/**
	 * Rollback the configuration, undoing any changes which have been
	 * made since the last commit or (if there are no commits) since the
	 * configuration was opened.
	 *
	 * @throws PoolsExecption If the rollback fails.
	 */
	public void rollback() throws PoolsException
	{
		if (PoolInternal.pool_conf_rollback(getConf()) !=
		    PoolInternal.PO_SUCCESS)
			throw new PoolsException();
	}

	/**
	 * Commit the configuration, making any changes since the configuration
	 * was last committed (or opened if there have been no prior commits)
	 * permanent.
	 *
	 * @throws PoolsExecption If the commit fails.
	 */
	public void commit(int active) throws PoolsException
	{
		if (PoolInternal.pool_conf_commit(getConf(), active) !=
		    PoolInternal.PO_SUCCESS)
			throw new PoolsException();
	}

	/**
	 * Export the configuration, storing the current state of the
	 * configuration at the supplied location in the supplied format.
	 *
	 * @param location The location of the export.
	 * @param format The format of the export.
	 * @throws PoolsExecption If the export fails.
	 */
	public void export(String location, int format) throws PoolsException
	{
		if (PoolInternal.pool_conf_export(getConf(), location, format)
		    != PoolInternal.PO_SUCCESS)
			throw new PoolsException();
	}

	/**
	 * Validate the configuration, ensuring that the current state of the
	 * configuration satisfies the supplied validation level.
	 *
	 * @param level The desired level of validation.
	 * @throws PoolsExecption If the validation fails.
	 */
	public void validate(int level) throws PoolsException
	{
		if (PoolInternal.pool_conf_validate(getConf(), level)
		    != PoolInternal.PO_SUCCESS)
			throw new PoolsException();
	}

	/**
	 * Update the configuration, ensuring that the current state of the
	 * configuration reflects that of the kernel.
	 *
	 * @throws PoolsExecption If the update fails.
	 * @return a bitmap of the changed types.
	 */
	public int update() throws PoolsException
	{
		return (PoolInternal.pool_conf_update(getConf()));
	}

	/**
	 * Create a pool with the supplied name.
	 *
	 * @param name The name of the PoolInternal.
	 * @throws PoolsExecption If the pool creation fails.
	 * @return a pool with the supplied name.
	 */
	public Pool createPool(String name) throws PoolsException
	{
		long aPool;

		if ((aPool = PoolInternal.pool_create(getConf(), name)) == 0) {
			throw new PoolsException();
		}
		Pool p = new Pool(this, aPool);
		elements.put(p.getKey(), p);
		return (p);
	}

	/**
	 * Destroy the supplied PoolInternal.
	 *
	 * @param aPool The pool to be destroyed.
	 * @throws PoolsException If the pool cannot be located.
	 */
	public void destroyPool(Pool aPool) throws PoolsException
	{
		elements.remove(aPool.getKey());
		PoolInternal.pool_destroy(getConf(), aPool.getPool());
	}

	/**
	 * Get the pool with the supplied name.
	 *
	 * @param name The name of the pool to be found.
	 * @throws PoolsExecption If the pool cannot be located.
	 * @return a pool with the supplied name.
	 */
	public Pool getPool(String name) throws PoolsException
	{
		long aPool;

		if ((aPool = PoolInternal.pool_get_pool(getConf(), name)) ==
		    0) {
			throw new PoolsException();
		}
		if (elements.containsKey("PoolInternal." + name))
			return ((Pool) elements.get("PoolInternal." + name));
		else {
			Pool p = new Pool(this, aPool);
			elements.put(p.getKey(), p);
			return (p);
		}
	}	

	/**
	 * Get the proxy for the pool with the supplied name.
	 *
	 * @param name The name of the pool to be found.
	 * @throws PoolsExecption If the pool cannot be located.
	 * @return the proxy for the pool with the supplied name.
	 */
	long checkPool(String name) throws PoolsException
	{
		long aPool;

		if ((aPool = PoolInternal.pool_get_pool(getConf(), name)) ==
		    0) {
			throw new PoolsException();
		}
		return (aPool);
	}	

	/**
	 * Get a list of pools which match the supplied selection criteria
	 * in values.
	 *
	 * @param values A list of values to be used to qualify the search.
	 * @throws PoolsExecption If there is an error executing the query.
	 * @return a list of pools which match the supplied criteria
	 */
	public List getPools(List values) throws PoolsException
	{
		List pools;

		if ((pools = PoolInternal.pool_query_pools(getConf(), values))
		    == null) {
			if (PoolInternal.pool_error() ==
			    PoolInternal.POE_INVALID_SEARCH)
				return new ArrayList();
			else
				throw new PoolsException();
		}
		ArrayList aList = new ArrayList(pools.size());
		for (int i = 0; i < pools.size(); i++)
			aList.add(new Pool(this,
			    ((Long)pools.get(i)).longValue()));
		return (aList);
	}
	
	/**
	 * Create a resource with the supplied type and name.
	 *
	 * @param type The type of the resource.
	 * @param name The name of the resource.
	 * @throws PoolsExecption If the resource creation fails.
	 * @return a resource of the supplied type and name.
	 */
	public Resource createResource(String type, String name)
	    throws PoolsException
	{
		long aResource;

		if ((aResource = PoolInternal.pool_resource_create(getConf(),
			 type, name)) == 0) {
			throw new PoolsException();
		}
		Resource res = new Resource(this, aResource);
		elements.put(res.getKey(), res);
		return (res);
	}

	/**
	 * Destroy the supplied resource.
	 *
	 * @param res The resource to be destroyed.
	 * @throws PoolsException If the resource cannot be located.
	 */
	public void destroyResource(Resource res) throws PoolsException
	{
		elements.remove(res.getKey());
		PoolInternal.pool_resource_destroy(getConf(),
		    res.getResource());
	}

	/**
	 * Get the resource with the supplied name.
	 *
	 * @param type The type of the resource to be found.
	 * @param name The name of the resource to be found.
	 * @throws PoolsExecption If the resource cannot be located.
	 * @return a resource with the supplied name.
	 */
	public Resource getResource(String type, String name)
	    throws PoolsException
	{
		long res;

		if ((res = PoolInternal.pool_get_resource(getConf(), type,
			 name)) == 0) {
			throw new PoolsException();
		}
		if (elements.containsKey(type + "." + name))
			return ((Resource) elements.get(type + "." + name));
		else {
			Resource r = new Resource(this, res);
			elements.put(r.getKey(), r);
			return (r);
		}
	}	

	/**
	 * Get the proxy for the resource with the supplied type and
	 * name.
	 *
	 * @param type The type of the resource to be found.
	 * @param name The name of the resource to be found.
	 * @throws PoolsExecption If the resource cannot be located.
	 * @return the proxy for the resource with the supplied name.
	 */
	long checkResource(String type, String name) throws PoolsException
	{
		long res;

		if ((res = PoolInternal.pool_get_resource(getConf(), type,
			 name)) == 0) {
			throw new PoolsException();
		}
		return (res);
	}	

	/**
	 * Get a list of resources which match the supplied selection criteria
	 * in values.
	 *
	 * @param values A list of values to be used to qualify the search.
	 * @throws PoolsExecption If there is an error executing the query.
	 * @return a list of resources which match the supplied criteria
	 */
	public List getResources(List values) throws PoolsException
	{
		List resources;

		if ((resources = PoolInternal.pool_query_resources(getConf(),
		    values)) == null) {
			if (PoolInternal.pool_error() ==
			    PoolInternal.POE_INVALID_SEARCH)
				return new ArrayList();
			else
				throw new PoolsException();
		}
		ArrayList aList = new ArrayList(resources.size());
		for (int i = 0; i < resources.size(); i++)
			aList.add(new Resource(this,
			    ((Long)resources.get(i)).longValue()));
		return (aList);
	}

	/**
	 * Get the component with the supplied name.
	 *
	 * @param type The type of the component to be found.
	 * @param sys_id The sys_id of the component to be found.
	 * @throws PoolsExecption If the component cannot be located.
	 * @return a component with the supplied name.
	 */
	public Component getComponent(String type, long sys_id)
	    throws PoolsException
	{
		List props = new ArrayList();
		Value ptype = new Value("type", type);
		Value psys_id = new Value(type + ".sys_id", sys_id);

		props.add(ptype);
		props.add(psys_id);
		List comps = getComponents(props);
		ptype.close();
		psys_id.close();
		if (comps.size() != 1)
			throw new PoolsException();
		return ((Component) comps.get(0));
	}	

	/**
	 * Get the component proxy with the supplied type and sys_id.
	 *
	 * @param type The type of the component to be found.
	 * @param sys_id The sys_id of the component to be found.
	 * @throws PoolsExecption If the component cannot be located.
	 * @return a component with the supplied name.
	 */
	long checkComponent(String type, long sys_id)
	    throws PoolsException
	{
		List props = new ArrayList();
		Value ptype = new Value("type", type);
		Value psys_id = new Value(type + ".sys_id", sys_id);

		props.add(ptype);
		props.add(psys_id);
		List comps = checkComponents(props);
		ptype.close();
		psys_id.close();
		if (comps.size() != 1)
			throw new PoolsException();
		return (((Long)comps.get(0)).longValue());
	}	

	/**
	 * Get a list of components which match the supplied selection criteria
	 * in values.
	 *
	 * @param values A list of values to be used to qualify the search.
	 * @throws PoolsExecption If there is an error executing the query.
	 * @return a list of components which match the supplied criteria
	 */
	public List getComponents(List values) throws PoolsException
	{
		List components;
		
		if ((components = PoolInternal.pool_query_components(getConf(),
		    values)) == null) {
			if (PoolInternal.pool_error() ==
			    PoolInternal.POE_INVALID_SEARCH)
				return new ArrayList();
			else
				throw new PoolsException();
		}
		ArrayList aList = new ArrayList(components.size());
		for (int i = 0; i < components.size(); i++) {
			/*
			 * Extract type information
			 */
			Value typeVal = new Value(name);

			if (PoolInternal.pool_get_property(getConf(),
			    ((Long)components.get(i)).longValue(), "type",
			    typeVal.getValue()) == PoolInternal.POC_INVAL)
				throw new PoolsException();
			if (typeVal == null)
				throw new PoolsException();
			String type = typeVal.getString();
			typeVal.close();

			Value idValue = new Value(name);

			if (PoolInternal.pool_get_property(getConf(),
			    ((Long)components.get(i)).longValue(),
			    type + ".sys_id", idValue.getValue()) ==
			    PoolInternal.POC_INVAL)
				throw new PoolsException();
			if (idValue == null)
				throw new PoolsException();
			long sys_id = idValue.getLong();
			idValue.close();
				
			if (elements.containsKey(type + "." + sys_id))
				aList.add((Component)elements.get(type + "." +
					  sys_id));
			else
				aList.add(new Component(this, ((Long)components.
							get(i)).longValue()));
		}
		return (aList);

	}

	/**
	 * Get a list of component proxies which match the supplied
	 * selection criteria in values.
	 *
	 * @param values A list of values to be used to qualify the search.
	 * @throws PoolsExecption If there is an error executing the query.
	 * @return a list of component proxies which match the
	 * supplied criteria
	 */
	List checkComponents(List values) throws PoolsException
	{
		List components;

		if ((components = PoolInternal.pool_query_components(getConf(),
		    values)) == null) {
			if (PoolInternal.pool_error() ==
			    PoolInternal.POE_INVALID_SEARCH)
				return new ArrayList();
			else
				throw new PoolsException();
		}
		return (components);
	}
	/**
	 * Returns a descriptive string which describes the configuration.
	 *
	 * @param deep Whether the information should contain information about
	 * all contained elements.
	 * @return a descriptive string which describes the configuration.
	 */
	public String getInformation(int deep)
	{
		return (PoolInternal.pool_conf_info(_conf.getConf(), deep));
	}

        /**
         * Returns a string representation of this configuration.
         *
         * @return  a string representation of this configuration.
         */
	public String toString()
	{
		StringBuffer buf = new StringBuffer();

		buf.append("system: ");
		buf.append(name);
		return (buf.toString());
	}

	/**
	 * Indicates whether some other Configuration is "equal to this one.
	 * @param o the reference object with which to compare.
	 * @return <code>true</code> if this object is the same as the
	 * o argument; <code>false</code> otherwise.
	 * @see	#hashCode()
	 */
	public boolean equals(Object o)
	{
		if (o == this)
			return (true);
		if (!(o instanceof Configuration))
			return (false);
		Configuration other = (Configuration) o;
		if (name.compareTo(other.getName()) != 0)
			return (false);
		return (true);
	}

	/**
	 * Returns a hash code value for the object. This method is
	 * supported for the benefit of hashtables such as those provided by
	 * <code>java.util.Hashtable</code>.
	 *
	 * @return a hash code value for this object.
	 * @see	#equals(java.lang.Object)
	 * @see	java.util.Hashtable
	 */
	public int hashCode()
	{
		return name.hashCode();
	}
	
	/**
	 * Return the pointer to this configuration as an element.
	 *
	 * @return The pointer to the native configuration which this object
	 * wraps.
	 * @throws PoolsExecption If there is an error converting the native
	 * configuration pointer to a native elem pointer.
	 */
	protected long getElem() throws PoolsException
	{
		long elem;

		if ((elem = PoolInternal.pool_conf_to_elem(getConf())) == 0)
			throw new PoolsException();
		return (elem);
	}

	/**
	 * Return the anme of the configuration.
	 */
	String getName()
	{
		return (name);
	}

	/**
	 * Return the key of the configuration.
	 */
	String getKey()
	{
		return (key);
	}
}

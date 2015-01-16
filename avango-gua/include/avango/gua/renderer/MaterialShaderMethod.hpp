#ifndef AVANGO_GUA_MATERIAL_SHADER_METHOD_HPP
#define AVANGO_GUA_MATERIAL_SHADER_METHOD_HPP

/**
 * \file
 * \ingroup av_gua
 */

#include <gua/renderer/MaterialShaderMethod.hpp>

#include <avango/gua/Fields.hpp>
#include <avango/FieldContainer.h>

namespace av
{
  namespace gua
  {
    /**
     * Wrapper for ::gua::MaterialShaderMethod
     *
     * \ingroup av_gua
     */
    class AV_GUA_DLL MaterialShaderMethod : public av::FieldContainer
    {
      AV_FC_DECLARE();

    public:

      /**
       * Constructor. When called without arguments, a new ::gua::MaterialShaderMethod is created.
       * Otherwise, the given ::gua::MaterialShaderMethod is used.
       */
      MaterialShaderMethod(::gua::MaterialShaderMethod const& guaMaterialShaderMethod =
                           ::gua::MaterialShaderMethod());



    public:

      SFString Source;
      SFString Name;

      virtual void getNameCB(const SFString::GetValueEvent& event);
      virtual void setNameCB(const SFString::SetValueEvent& event);

      virtual void getSourceCB(const SFString::GetValueEvent& event);
      virtual void setSourceCB(const SFString::SetValueEvent& event);



      /**
       * Get the wrapped ::gua::MaterialShaderMethod.
       */

      template <typename T>
      void set_uniform(std::string const& name, T const& value) {
        m_guaMaterialShaderMethod.set_uniform(name, value);
      }

      void load_from_file(std::string const& file) {
        m_guaMaterialShaderMethod.load_from_file(file);
      }

      void load_from_json(std::string const& json) {
        m_guaMaterialShaderMethod.load_from_json(json);
      }

      ::gua::MaterialShaderMethod const& getGuaMaterialShaderMethod() const;

    private:

      ::gua::MaterialShaderMethod m_guaMaterialShaderMethod;

      MaterialShaderMethod(const MaterialShaderMethod&);
      MaterialShaderMethod& operator=(const MaterialShaderMethod&);
    };

    typedef SingleField<Link<MaterialShaderMethod> > SFMaterialShaderMethod;
    typedef MultiField<Link<MaterialShaderMethod> > MFMaterialShaderMethod;

  }

#ifdef AV_INSTANTIATE_FIELD_TEMPLATES
  template class AV_GUA_DLL SingleField<Link<gua::MaterialShaderMethod> >;
  template class AV_GUA_DLL MultiField<Link<gua::MaterialShaderMethod> >;
#endif

}

#endif //AVANGO_GUA_MATERIAL_SHADER_METHOD_HPP
